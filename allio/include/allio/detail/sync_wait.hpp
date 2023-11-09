#pragma once

#include <allio/detail/event_queue.hpp>
#include <allio/detail/execution.hpp>
#include <allio/detail/get_multiplexer.hpp>
#include <allio/detail/handles/event_handle.hpp>

#include <vsm/assert.h>
#include <vsm/intrusive/mpsc_queue.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>
#include <vsm/tag_ptr.hpp>
#include <vsm/utility.hpp>

#include <exec/repeat_effect_until.hpp>
#include <exec/when_any.hpp>

#include <optional>
#include <thread>

namespace allio::detail {
namespace _sync_wait {

template<typename MultiplexerHandle>
class event_queue
{
	struct operation_base;

	struct operation_virtual_table
	{
		void(*set_value)(operation_base&);
		void(*set_stopped)(operation_base&);
	};

	struct operation_base : vsm::intrusive::mpsc_queue_link
	{
		using ptr_type = vsm::tag_ptr<operation_virtual_table const, bool>;

		vsm::atomic<ptr_type> virtual_table_and_cancel;

		explicit operation_base(operation_virtual_table const& virtual_table)
			: virtual_table_and_cancel(ptr_type(&virtual_table, false))
		{
		}

		operation_base(operation_base const&) = delete;
		operation_base& operator=(operation_base const&) = delete;

		void process()
		{
			auto const p = virtual_table_and_cancel.load(std::memory_order_acquire);
			(p.tag() ? p->set_stopped : p->set_value)(*this);
		}
	};


	using event_handle_type = async_handle<event_t, MultiplexerHandle>;

	event_handle_type m_event;
	vsm::intrusive::mpsc_queue<operation_base> m_mpsc_queue;
	vsm::intrusive::forward_list<operation_base> m_forward_list;

public:
	template<std::convertible_to<MultiplexerHandle> Multiplexer>
	[[nodiscard]] static vsm::result<event_queue> create(Multiplexer&& multiplexer)
	{
		vsm_try(event, _event_blocking::create_event(auto_reset_event));
		vsm_try(async_event, vsm_move(event).via(vsm_forward(multiplexer)));
		return vsm_lazy(event_queue(vsm_move(async_event)));
	}

	[[nodiscard]] MultiplexerHandle const& multiplexer() const
	{
		return m_event.multiplexer();
	}


	class event : operation_base
	{
		using operation_base::operation_base;
		friend event_queue;
	};

	template<typename Implementation>
	class event_base : public event
	{
	protected:
		event_base()
			: event(virtual_table)
		{
		}

	private:
		static void _set_value(operation_base& self)
		{
			static_cast<Implementation&>(self).set_value();
		}

		static void _set_stopped(operation_base& self)
		{
			static_cast<Implementation&>(self).set_stopped();
		}

		static constexpr operation_virtual_table virtual_table =
		{
			.set_value = _set_value,
			.set_stopped = _set_stopped,
		};
	};

	void schedule(event& event) &
	{
		if (m_mpsc_queue.push_back(&event))
		{
			vsm_verify(m_event.signal());
		}
	}

	void schedule_relaxed(event& event) &
	{
		m_forward_list.push_back(&event);
	}


	auto receive_events()
	{
		return m_event.wait() | ex::then([&]()
		{
			m_forward_list.splice_back(m_mpsc_queue.pop_all_reversed());
		});
	}

	bool process_events()
	{
		if (m_forward_list.empty())
		{
			return false;
		}

		do
		{
			m_forward_list.pop_front()->process();
		}
		while (!m_forward_list.empty());

		return true;
	}

private:
	explicit event_queue(event_handle_type&& event)
		: m_event(vsm_move(event))
	{
	}
};

template<typename MultiplexerHandle>
struct context
{
	event_queue<MultiplexerHandle>& event_queue;
	std::thread::id thread_id;

	class scheduler
	{
		class sender
		{
			class env
			{
				context* m_context;

			public:
				explicit env(context& context) noexcept
					: m_context(&context)
				{
				}

				template<typename CPO>
				friend scheduler tag_invoke(ex::get_completion_scheduler_t<CPO>, env const& self) noexcept
				{
					return scheduler(*self.m_context);
				}
			};

			template<typename E>
			using event_base = typename _sync_wait::event_queue<MultiplexerHandle>::template event_base<E>;

			template<typename R>
			class operation : event_base<operation<R>>
			{
				context* m_context;
				vsm_no_unique_address R m_receiver;

			public:
				explicit operation(context& context, auto&& receiver)
					: m_context(&context)
					, m_receiver(vsm_forward(receiver))
				{
				}

				friend void tag_invoke(ex::start_t, operation& self) noexcept
				{
					if (std::this_thread::get_id() == self.m_context->thread_id)
					{
						self.m_context->event_queue.schedule_relaxed(self);
					}
					else
					{
						self.m_context->event_queue.schedule(self);
					}
				}

			private:
				void set_value()
				{
					ex::set_value(vsm_move(m_receiver));
				}

				void set_stopped()
				{
					ex::set_stopped(vsm_move(m_receiver));
				}

				friend event_base<operation<R>>;
			};

			context* m_context;

		public:
			using is_sender = void;

			using completion_signatures = ex::completion_signatures<
				ex::set_value_t(),
				ex::set_stopped_t()
			>;

			explicit sender(context& context) noexcept
				: m_context(&context)
			{
			}

			template<typename R>
			friend operation<std::decay_t<R>> tag_invoke(ex::connect_t, sender const& self, R&& receiver)
			{
				return operation<std::decay_t<R>>(*self.m_context, vsm_forward(receiver));
			}

			friend env tag_invoke(ex::get_env_t, sender const& self) noexcept
			{
				return env(*self.m_context);
			}
		};

		context* m_context;

	public:
		explicit scheduler(context& context) noexcept
			: m_context(&context)
		{
		}

		friend sender tag_invoke(ex::schedule_t, scheduler const& self) noexcept
		{
			return sender(*self.m_context);
		}

		friend bool operator==(scheduler const&, scheduler const&) = default;
	};

	class env
	{
		context* m_context;

	public:
		explicit env(context& context)
			: m_context(&context)
		{
		}

		friend auto tag_invoke(ex::get_scheduler_t, env const& self) noexcept
		{
			return scheduler(*self.m_context);
		}

		friend auto tag_invoke(ex::get_delegatee_scheduler_t, env const& self) noexcept
		{
			return scheduler(*self.m_context);
		}

		friend MultiplexerHandle tag_invoke(get_multiplexer_t, env const& self) noexcept
		{
			return self.m_context->multiplexer;
		}
	};
};

template<typename MultiplexerHandle, typename Variant>
struct context_and_variant : context<MultiplexerHandle>
{
	Variant variant;
};

template<typename MultiplexerHandle, typename Sender, typename Continuation>
using _result =
	ex::__try_value_types_of_t<
		Sender,
		typename context<MultiplexerHandle>::env,
		ex::__transform<ex::__q<ex::__decay_t>, Continuation>,
		ex::__q<ex::__msingle>>;

template<typename MultiplexerHandle, typename Sender>
using result =
	ex::__mtry_eval<
		_result,
		MultiplexerHandle,
			Sender,
			ex::__q<std::tuple>>;

template<typename Result>
using variant = std::variant<
	std::monostate,
	Result,
	ex::set_stopped_t,
	std::exception_ptr>;

template<typename MultiplexerHandle, typename Variant>
struct receiver
{
	class type
	{
		context_and_variant<MultiplexerHandle, Variant>* m_context_and_variant;

	public:
		using is_receiver = void;

		explicit type(context_and_variant<MultiplexerHandle, Variant>& context_and_variant)
			: m_context_and_variant(&context_and_variant)
		{
		}

		friend void tag_invoke(ex::set_value_t, type&& self, auto&&... values) noexcept
		{
			vsm_assert(self.m_context_and_variant->variant.index() == 0);
			try
			{
				self.m_context_and_variant->variant.template emplace<1>(vsm_forward(values)...);
			}
			catch (...)
			{
				self.set_error(std::current_exception());
			}
		}

		friend void tag_invoke(ex::set_error_t, type&& self, auto&& error) noexcept
		{
			vsm_assert(self.m_context_and_variant->variant.index() == 0);
			self.set_error(vsm_forward(error));
		}

		friend void tag_invoke(ex::set_stopped_t, type&& self) noexcept
		{
			vsm_assert(self.m_context_and_variant->variant.index() == 0);
			self.m_context_and_variant->variant.template emplace<2>();
		}

		friend typename context<MultiplexerHandle>::env tag_invoke(ex::get_env_t, type const& self) noexcept
		{
			return typename context<MultiplexerHandle>::env(*self.m_context_and_variant);
		}

	private:
		void set_error(auto&& error) noexcept
		{
			using error_type = std::decay_t<decltype(error)&&>;
			if constexpr (std::is_same_v<error_type, std::exception_ptr>)
			{
				m_context_and_variant->variant.template emplace<3>(vsm_forward(error));
			}
			else if constexpr (std::is_same_v<error_type, std::error_code>)
			{
				m_context_and_variant->variant.template emplace<3>(std::make_exception_ptr(std::system_error(error)));
			}
			else
			{
				m_context_and_variant->variant.template emplace<3>(std::make_exception_ptr(vsm_forward(error)));
			}
		}
	};
};

template<typename MultiplexerHandle, typename Sender>
auto sync_wait(event_queue<MultiplexerHandle>& event_queue, Sender&& sender)
	-> std::optional<result<MultiplexerHandle, Sender&&>>
{
	using variant_type = variant<result<MultiplexerHandle, Sender&&>>;

	context_and_variant<MultiplexerHandle, variant_type> context_and_variant
	{
		{ event_queue, std::this_thread::get_id() }
	};

	auto const is_completed = [&]() -> bool
	{
		return context_and_variant.variant.index() != 0;
	};

	auto operation_state = ex::connect(
		exec::when_any(
			vsm_forward(sender),
			exec::repeat_effect_until(event_queue.receive_events() | ex::then(is_completed))),
		typename receiver<MultiplexerHandle, variant_type>::type(context_and_variant));

	ex::start(operation_state);

	while (!is_completed())
	{
		if (!event_queue.process_events())
		{
			unrecoverable(vsm::discard_value(poll_io(context_and_variant.event_queue.multiplexer())));
		}
	}

	if (context_and_variant.variant.index() == 2)
	{
		return std::nullopt;
	}

	if (context_and_variant.variant.index() == 3)
	{
		std::rethrow_exception(std::get<3>(vsm_move(context_and_variant.variant)));
	}

	return std::get<1>(vsm_move(context_and_variant.variant));
}

} // namespace _sync_wait

struct sync_wait_t
{
	template<typename Multiplexer, typename Sender>
	auto vsm_static_operator_invoke(Multiplexer&& multiplexer, Sender&& sender)
		-> std::optional<_sync_wait::result<multiplexer_handle_t<Multiplexer>, Sender&&>>
	{
		using multiplexer_handle_type = multiplexer_handle_t<Multiplexer>;
		auto queue = _sync_wait::event_queue<multiplexer_handle_type>::create(vsm_forward(multiplexer));
		return _sync_wait::sync_wait(queue.value(), vsm_forward(sender));
	}
};
inline constexpr sync_wait_t sync_wait = {};

#if 0
namespace _io_context {

template<typename MultiplexerHandle, typename Sender, typename Continuation>
using _sync_wait_result =
	ex::__try_value_types_of_t<
		Sender,
		typename context<MultiplexerHandle>::env,
		ex::__transform<ex::__q<ex::__decay_t>, Continuation>,
		ex::__q<ex::__msingle>>;

template<typename MultiplexerHandle, typename Sender>
using sync_wait_result =
	ex::__mtry_eval<
		_sync_wait_result,
		MultiplexerHandle,
			Sender,
			ex::__q<std::tuple>>;

template<typename Result>
using sync_wait_variant = std::variant<
	std::monostate,
	Result,
	ex::set_stopped_t,
	std::exception_ptr>;

template<typename MultiplexerHandle>
class io_context
{
	struct operation_base;

	struct operation_virtual_table
	{
		void(*set_value)(operation_base&);
		void(*set_stopped)(operation_base&);
	};

	struct operation_base : vsm::intrusive::mpsc_queue_link
	{
		using ptr_type = vsm::tag_ptr<operation_virtual_table const, bool>;

		vsm::atomic<ptr_type> virtual_table_and_cancel;

		explicit operation_base(operation_virtual_table const& virtual_table)
			: virtual_table_and_cancel(ptr_type(&virtual_table, false))
		{
		}

		operation_base(operation_base const&) = delete;
		operation_base& operator=(operation_base const&) = delete;
	};


	using event_handle_type = async_handle<event_t, MultiplexerHandle>;

	event_handle_type m_event;
	vsm::intrusive::mpsc_queue<operation_base> m_mpsc_queue;
	vsm::intrusive::forward_list<operation_base> m_forward_list;


	struct sync_wait_context
	{
		io_context& io_context;
		std::thread::id thread_id;

		class scheduler
		{
			class sender
			{
				class env
				{
					sync_wait_context* m_context;

				public:
					explicit env(sync_wait_context& context) noexcept
						: m_context(&context)
					{
					}

					template<typename CPO>
					friend scheduler tag_invoke(ex::get_completion_scheduler_t<CPO>, env const& self) noexcept
					{
						return scheduler(*self.m_context);
					}
				};

				template<typename R>
				class operation : decltype(event_queue)::template event<operation<R>>
				{
					sync_wait_context* m_context;
					vsm_no_unique_address R m_receiver;

				public:
					explicit operation(sync_wait_context& context, auto&& receiver)
						: m_context(&context)
						, m_receiver(vsm_forward(receiver))
					{
					}

					friend void tag_invoke(ex::start_t, operation& self) noexcept
					{
						io_context& io_context = self.m_context->io_context;
						if (std::this_thread::get_id() == self.m_context->thread_id)
						{
							io_context.m_forward_list.push_back(&self);
						}
						else if (io_context.m_mpsc_queue.push_back(&self))
						{
							vsm_verify(io_context.m_event.signal());
						}
					}

				private:
					void set_value()
					{
						ex::set_value(vsm_move(m_receiver));
					}

					void set_stopped()
					{
						ex::set_stopped(vsm_move(m_receiver));
					}

					friend typename decltype(event_queue)::template event<operation<R>>;
				};

				sync_wait_context* m_context;

			public:
				using is_sender = void;

				using completion_signatures = ex::completion_signatures<
					ex::set_value_t(),
					ex::set_stopped_t()
				>;

				explicit sender(sync_wait_context& context) noexcept
					: m_context(&context)
				{
				}

				template<typename R>
				friend operation<std::decay_t<R>> tag_invoke(ex::connect_t, sender const& self, R&& receiver)
				{
					return operation<std::decay_t<R>>(*self.m_context, vsm_forward(receiver));
				}

				friend env tag_invoke(ex::get_env_t, sender const& self) noexcept
				{
					return env(*self.m_context);
				}
			};

			sync_wait_context* m_context;

		public:
			explicit scheduler(sync_wait_context& context) noexcept
				: m_context(&context)
			{
			}

			friend sender tag_invoke(ex::schedule_t, scheduler const& self) noexcept
			{
				return sender(*self.m_context);
			}

			friend bool operator==(scheduler const&, scheduler const&) = default;
		};

		class env
		{
			sync_wait_context* m_context;

		public:
			explicit env(sync_wait_context& context)
				: m_context(&context)
			{
			}

			friend auto tag_invoke(ex::get_scheduler_t, env const& self) noexcept
			{
				return scheduler(*self.m_context);
			}

			friend auto tag_invoke(ex::get_delegatee_scheduler_t, env const& self) noexcept
			{
				return scheduler(*self.m_context);
			}

			friend MultiplexerHandle tag_invoke(get_multiplexer_t, env const& self) noexcept
			{
				return self.m_context->io_context.m_event.multiplexer();
			}
		};
	};

	template<typename Variant>
	struct sync_wait_context_and_variant : sync_wait_context
	{
		Variant variant;
	};

	template<typename Variant>
	class sync_wait_receiver
	{
		using context_and_variant_type = sync_wait_context_and_variant<Variant>;

		context_and_variant_type* m_context_and_variant;

	public:
		using is_receiver = void;

		explicit type(context_and_variant_type& context_and_variant)
			: m_context_and_variant(&context_and_variant)
		{
		}

		friend void tag_invoke(ex::set_value_t, type&& self, auto&&... values) noexcept
		{
			vsm_assert(self.m_context_and_variant->variant.index() == 0);
			try
			{
				self.m_context_and_variant->variant.template emplace<1>(vsm_forward(values)...);
			}
			catch (...)
			{
				self.set_error(std::current_exception());
			}
		}

		friend void tag_invoke(ex::set_error_t, type&& self, auto&& error) noexcept
		{
			vsm_assert(self.m_context_and_variant->variant.index() == 0);
			self.set_error(vsm_forward(error));
		}

		friend void tag_invoke(ex::set_stopped_t, type&& self) noexcept
		{
			vsm_assert(self.m_context_and_variant->variant.index() == 0);
			self.m_context_and_variant->variant.template emplace<2>();
		}

		friend typename context_type::env tag_invoke(ex::get_env_t, type const& self) noexcept
		{
			return typename context_type::env(self.m_context_and_variant->context);
		}

	private:
		void set_error(auto&& error) noexcept
		{
			using error_type = std::decay_t<decltype(error)&&>;
			if constexpr (std::is_same_v<error_type, std::exception_ptr>)
			{
				m_context_and_variant->variant.template emplace<3>(vsm_forward(error));
			}
			else if constexpr (std::is_same_v<error_type, std::error_code>)
			{
				m_context_and_variant->variant.template emplace<3>(std::make_exception_ptr(std::system_error(error)));
			}
			else
			{
				m_context_and_variant->variant.template emplace<3>(std::make_exception_ptr(vsm_forward(error)));
			}
		}
	};

public:
	class event : operation_base
	{
		using operation_base::operation_base;
		friend io_context;
	};

	template<typename Implementation>
	class event_base : public event
	{
	protected:
		event_base()
			: event(virtual_table)
		{
		}

	private:
		static void _set_value(operation_base& self)
		{
			static_cast<Implementation&>(self).set_value();
		}

		static void _set_stopped(operation_base& self)
		{
			static_cast<Implementation&>(self).set_stopped();
		}

		static constexpr operation_virtual_table virtual_table =
		{
			.set_value = _set_value,
			.set_stopped = _set_stopped,
		};
	};


	template<std::convertible_to<MultiplexerHandle> Multiplexer>
	static vsm::result<io_context> create(Multiplexer&& multiplexer)
	{
		vsm_try(event, create_event(auto_reset_event));
		return vsm_lazy(io_context(vsm_move(event)));
	}


	template<typename Sender>
	auto sync_wait(Sender&& sender)
		-> std::optional<sync_wait_result<MultiplexerHandle, Sender&&>>
	{
		using variant_type = sync_wait_variant<sync_wait_result<MultiplexerHandle, Sender&&>>;

		sync_wait_context_and_variant<variant_type> context_and_variant
		{
			{ *this, std::this_thread::get_id() }
		};

		auto operation_state = ex::connect(
			vsm_forward(sender),
			sync_wait_receiver<variant_type>(context));

		ex::start(operation_state);

		while (context_and_variant.variant.index() == 0)
		{
			// The multiplexer is polled until the specified sender completes.
			unrecoverable(vsm::discard_value(poll_io(m_event.multiplexer())));
		}

		if (context_and_variant.variant.index() == 2)
		{
			return std::nullopt;
		}

		if (context_and_variant.variant.index() == 3)
		{
			std::rethrow_exception(std::get<3>(vsm_move(context_and_variant.variant)));
		}

		return std::get<1>(vsm_move(context_and_variant.variant));
	}

private:
	explicit io_context(event_handle_type&& event)
		: m_event(vsm_move(event))
	{
	}
};

} // namespace _io_context

using _io_context::io_context;


namespace _sync_wait {

template<typename MultiplexerHandle>
class event
{
	async_event_handle<MultiplexerHandle> m_event;

public:
	/*explicit*/ event(MultiplexerHandle multiplexer_handle)
		: m_event(create_event(multiplexer_handle, auto_reset_event).value())
	{
	}

	void operator()() const
	{
		unrecoverable(m_event.signal());
	}
};

template<typename MultiplexerHandle>
struct context
{
	vsm_no_unique_address MultiplexerHandle multiplexer;
	basic_event_queue<event<MultiplexerHandle>> event_queue;
	std::thread::id thread_id;

	template<std::convertible_to<MultiplexerHandle> M>
	explicit context(M&& m)
		: multiplexer(vsm_forward(m))
		, event_queue(multiplexer)
		, thread_id(std::this_thread::get_id())
	{
	}

	class scheduler
	{
		class sender
		{
			class env
			{
				context* m_context;

			public:
				explicit env(context& context) noexcept
					: m_context(&context)
				{
				}

				template<typename CPO>
				friend scheduler tag_invoke(ex::get_completion_scheduler_t<CPO>, env const& self) noexcept
				{
					return scheduler(*self.m_context);
				}
			};

			template<typename R>
			class operation : decltype(event_queue)::template event<operation<R>>
			{
				context* m_context;
				vsm_no_unique_address R m_receiver;

			public:
				explicit operation(context& context, auto&& receiver)
					: m_context(&context)
					, m_receiver(vsm_forward(receiver))
				{
				}

				friend void tag_invoke(ex::start_t, operation& self) noexcept
				{
					if (std::this_thread::get_id() == self.m_context->thread_id)
					{
						self.m_context->event_queue.schedule_relaxed(self);
					}
					else
					{
						self.m_context->event_queue.schedule(self);
					}
				}

			private:
				void set_value()
				{
					ex::set_value(vsm_move(m_receiver));
				}

				void set_stopped()
				{
					ex::set_stopped(vsm_move(m_receiver));
				}

				friend typename decltype(event_queue)::template event<operation<R>>;
			};

			context* m_context;

		public:
			using is_sender = void;

			using completion_signatures = ex::completion_signatures<
				ex::set_value_t(),
				ex::set_stopped_t()
			>;

			explicit sender(context& context) noexcept
				: m_context(&context)
			{
			}

			template<typename R>
			friend operation<std::decay_t<R>> tag_invoke(ex::connect_t, sender const& self, R&& receiver)
			{
				return operation<std::decay_t<R>>(*self.m_context, vsm_forward(receiver));
			}

			friend env tag_invoke(ex::get_env_t, sender const& self) noexcept
			{
				return env(*self.m_context);
			}
		};

		context* m_context;

	public:
		explicit scheduler(context& context) noexcept
			: m_context(&context)
		{
		}

		friend sender tag_invoke(ex::schedule_t, scheduler const& self) noexcept
		{
			return sender(*self.m_context);
		}

		friend bool operator==(scheduler const&, scheduler const&) = default;
	};

	class env
	{
		context* m_context;

	public:
		explicit env(context& context)
			: m_context(&context)
		{
		}

		friend auto tag_invoke(ex::get_scheduler_t, env const& self) noexcept
		{
			return scheduler(*self.m_context);
		}

		friend auto tag_invoke(ex::get_delegatee_scheduler_t, env const& self) noexcept
		{
			return scheduler(*self.m_context);
		}

		friend MultiplexerHandle tag_invoke(get_multiplexer_t, env const& self) noexcept
		{
			return self.m_context->multiplexer;
		}
	};
};

template<typename MultiplexerHandle, typename Sender, typename Continuation>
using _result =
	ex::__try_value_types_of_t<
		Sender,
		typename context<MultiplexerHandle>::env,
		ex::__transform<ex::__q<ex::__decay_t>, Continuation>,
		ex::__q<ex::__msingle>>;

template<typename MultiplexerHandle, typename Sender>
using result =
	ex::__mtry_eval<
	_result,
	MultiplexerHandle,
		Sender,
		ex::__q<std::tuple>>;

template<typename Result>
using variant = std::variant<
	std::monostate,
	Result,
	ex::set_stopped_t,
	std::exception_ptr>;

template<typename Context, typename Variant>
struct context_and_variant
{
	Context context;
	Variant variant;
};

template<typename MultiplexerHandle, typename Result>
struct receiver
{
	class type
	{
		using context_type = context<MultiplexerHandle>;
		using variant_type = variant<Result>;
		using context_and_variant_type = context_and_variant<context_type, variant_type>;

		context_and_variant_type* m_context_and_variant;

	public:
		using is_receiver = void;

		explicit type(context_and_variant_type& context_and_variant)
			: m_context_and_variant(&context_and_variant)
		{
		}

		friend void tag_invoke(ex::set_value_t, type&& self, auto&&... values) noexcept
		{
			vsm_assert(self.m_context_and_variant->variant.index() == 0);
			try
			{
				self.m_context_and_variant->variant.template emplace<1>(vsm_forward(values)...);
			}
			catch (...)
			{
				self.set_error(std::current_exception());
			}
		}

		friend void tag_invoke(ex::set_error_t, type&& self, auto&& error) noexcept
		{
			vsm_assert(self.m_context_and_variant->variant.index() == 0);
			self.set_error(vsm_forward(error));
		}

		friend void tag_invoke(ex::set_stopped_t, type&& self) noexcept
		{
			vsm_assert(self.m_context_and_variant->variant.index() == 0);
			self.m_context_and_variant->variant.template emplace<2>();
		}

		friend typename context_type::env tag_invoke(ex::get_env_t, type const& self) noexcept
		{
			return typename context_type::env(self.m_context_and_variant->context);
		}

	private:
		void set_error(auto&& error) noexcept
		{
			using error_type = std::decay_t<decltype(error)&&>;
			if constexpr (std::is_same_v<error_type, std::exception_ptr>)
			{
				m_context_and_variant->variant.template emplace<3>(vsm_forward(error));
			}
			else if constexpr (std::is_same_v<error_type, std::error_code>)
			{
				m_context_and_variant->variant.template emplace<3>(std::make_exception_ptr(std::system_error(error)));
			}
			else
			{
				m_context_and_variant->variant.template emplace<3>(std::make_exception_ptr(vsm_forward(error)));
			}
		}
	};
};

template<typename MultiplexerHandle, typename Result>
using receiver_t = typename receiver<MultiplexerHandle, Result>::type;

struct sync_wait_t
{
	template<typename Multiplexer, typename Sender>
	auto vsm_static_operator_invoke(Multiplexer&& multiplexer, Sender&& sender)
		-> std::optional<result<multiplexer_handle_t<Multiplexer>, Sender&&>>
	{
		using multiplexer_handle_type = multiplexer_handle_t<Multiplexer>;
		using result_type = result<multiplexer_handle_type, Sender&&>;

		using context_type = context<multiplexer_handle_type>;
		using variant_type = variant<result_type>;

		context_and_variant<context_type, variant_type> context_and_variant
		{
			// The specified multiplexer reference is only used once
			// to initialise the multiplexer handle held by the context object.
			.context = context_type(vsm_forward(multiplexer))
		};

		auto operation_state = ex::connect(
			vsm_forward(sender),
			receiver_t<multiplexer_handle_type, result_type>(context_and_variant));

		ex::start(operation_state);

		while (context_and_variant.variant.index() == 0)
		{
			// The multiplexer is polled until the specified sender completes.
			unrecoverable(vsm::discard_value(poll_io(context_and_variant.context.multiplexer)));
		}

		if (context_and_variant.variant.index() == 2)
		{
			return std::nullopt;
		}

		if (context_and_variant.variant.index() == 3)
		{
			std::rethrow_exception(std::get<3>(vsm_move(context_and_variant.variant)));
		}

		return std::get<1>(vsm_move(context_and_variant.variant));
	}
};

} // namespace _sync_wait

struct sync_wait_t
{
	template<typename Multiplexer, typename Sender>
	auto vsm_static_operator_invoke(Multiplexer&& multiplexer, Sender&& sender)
		-> std::optional<_io_context::sync_wait_result<multiplexer_handle_t<Multiplexer>, Sender&&>>
	{
		io_context<multiplexer_handle_t<Multiplexer>>::create(vsm_forward(multiplexer)).value().sync_wait(vsm_forward(sender));
	}
};
inline constexpr sync_wait_t sync_wait = {};
#endif

} // namespace allio::detail
