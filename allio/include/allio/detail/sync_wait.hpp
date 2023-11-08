#pragma once

#include <allio/detail/event_queue.hpp>
#include <allio/detail/execution.hpp>
#include <allio/detail/get_multiplexer.hpp>
#include <allio/detail/handles/event_handle.hpp>

#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>
#include <vsm/utility.hpp>

#include <optional>

namespace allio::detail {
namespace _io_context {

template<typename MultiplexerHandle>
class io_context
{
	async_event_handle<MultiplexerHandle> m_event;
	vsm::intrusive::mpsc_queue<operation_base> m_mpsc_queue;
	vsm::intrusive::forward_list<operation_base> m_forward_list;

	template<typename Variant>
	struct sync_wait_context
	{
		io_context& context;
		std::thread::id thread_id;
		Variant variant;
	};

public:
	template<typename Sender>
	auto sync_wait(Sender&& sender)
	{
		using result_type = result<multiplexer_handle_type, Sender&&>;
		using variant_type = variant<result_type>;

		sync_wait_context<variant_type> context
		{
			.context = *this,
			.thread_id = std::this_thread::get_id(),
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

} // namespace _io_context

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

using _sync_wait::sync_wait_t;
inline constexpr sync_wait_t sync_wait = {};

} // namespace allio::detail
