#pragma once

#include <allio/detail/execution.hpp>
#include <allio/detail/handle.hpp>
#include <allio/detail/io.hpp>
#include <allio/get_multiplexer.hpp>

#include <vsm/standard.hpp>
#include <vsm/type_traits.hpp>

#include <optional>

namespace allio::detail {

template<typename E>
using environment_multiplexer_handle_t = std::remove_cvref_t<decltype(get_multiplexer(std::declval<E>()))>;

template<bool>
struct _set_value_signature;

template<>
struct _set_value_signature<0>
{
	template<typename R>
	using type = ex::set_value_t(R);
};

template<>
struct _set_value_signature<1>
{
	template<typename R>
	using type = ex::set_value_t();
};

template<typename R>
using set_value_signature = typename _set_value_signature<std::is_void_v<R>>::template type<R>;


template<handle Handle, operation_c Operation>
class io_sender
{
	using object_type = typename Handle::object_type;
	using handle_type = handle_const_t<Operation, Handle>;

	using multiplexer_handle_type = typename Handle::multiplexer_handle_type;
	using multiplexer_type = typename multiplexer_handle_type::multiplexer_type;

	using operation_type = async_operation_t<multiplexer_type, object_type, Operation>;
	using io_status_type = typename multiplexer_type::io_status_type;

	using params_type = io_parameters_t<object_type, Operation>;
	using result_type = io_result_t<Handle, Operation>;

	template<typename Receiver>
	class operation
		: io_handler_base<multiplexer_type, operation<Receiver>>
		, ex::__in_place_stoppable_base<operation<Receiver>>
	{
		using io_handler_type = io_handler<multiplexer_type>;
		using stoppable_base = ex::__in_place_stoppable_base<operation<Receiver>>;

		handle_type& m_handle;
		vsm_no_unique_address params_type m_args;
		vsm_no_unique_address operation_type m_operation;
		vsm_no_unique_address Receiver m_receiver;

	public:
		explicit operation(auto&& sender, auto&& receiver)
			noexcept(noexcept(Receiver(vsm_forward(receiver))))
			: m_handle(*sender.m_handle)
			, m_args(vsm_forward(sender).m_args)
			, m_receiver(vsm_forward(receiver))
		{
		}

	private:
		friend void tag_invoke(ex::start_t, operation& self) noexcept
		{
			auto r = submit_io(
				self.m_handle,
				self.m_operation,
				vsm_as_const(self.m_args),
				static_cast<io_handler_type&>(self));

			if (r.is_pending())
			{
				// This is not a stream sender.
				vsm_assert(!r.has_value());

				self.stoppable_base::register_stoppable(
					ex::get_stop_token(ex::get_env(self.m_receiver)));
			}
			else
			{
				self.handle_result(vsm_move(r));
			}
		}

		void notify(io_status_type&& status) noexcept
		{
			auto r = notify_io(
				m_handle,
				m_operation,
				vsm_as_const(m_args),
				vsm_move(status));

			// This is not a stream sender.
			vsm_assert(!r.is_pending());

			stoppable_base::deregister_stoppable();

			handle_result(vsm_move(r));
		}

		template<typename R>
		void handle_result(io_result<R>&& r)
		{
			if (r.has_value())
			{
				if constexpr (std::is_void_v<R>)
				{
					ex::set_value(vsm_move(m_receiver));
				}
				else
				{
					if constexpr (std::is_same_v<R, result_type>)
					{
						ex::set_value(vsm_move(m_receiver), vsm_move(*r));
					}
					else
					{
						if (auto r2 = rebind_handle<result_type>(vsm_move(*r)))
						{
							ex::set_value(vsm_move(m_receiver), vsm_move(*r2));
						}
						else
						{
							ex::set_error(vsm_move(m_receiver), r2.error());
						}
					}
				}
			}
			else if (r.is_cancelled())
			{
				ex::set_stopped(vsm_move(m_receiver));
			}
			else
			{
				ex::set_error(vsm_move(m_receiver), r.error());
			}
		}

		void on_stop_requested()
		{
			cancel_io(m_handle, m_operation);
		}

		friend io_handler_base<multiplexer_type, operation<Receiver>>;
		friend stoppable_base;
	};

	handle_type* m_handle;
	vsm_no_unique_address params_type m_args;

public:
	using is_sender = void;

	using completion_signatures = ex::completion_signatures<
		set_value_signature<result_type>,
		ex::set_error_t(std::error_code),
		ex::set_stopped_t()
	>;

	template<std::convertible_to<params_type> Args>
	explicit io_sender(handle_type& handle, Args&& args)
		: m_handle(&handle)
		, m_args(vsm_forward(args))
	{
	}

	template<ex::receiver Receiver>
	friend operation<std::decay_t<Receiver>> tag_invoke(
		ex::connect_t,
		vsm::any_cvref_of<io_sender> auto&& sender,
		Receiver&& receiver)
	{
		return operation<std::decay_t<Receiver>>(vsm_forward(sender), vsm_forward(receiver));
	}
};


template<object Object, producer Operation, template<typename> typename HandleTemplate>
class io_handle_sender
{
	using params_type = io_parameters_t<Object, Operation>;

	template<multiplexer_handle_for<Object> MultiplexerHandle, ex::receiver Receiver>
	class operation
		: io_handler_base<
			typename MultiplexerHandle::multiplexer_type,
			operation<MultiplexerHandle, Receiver>>
		, ex::__in_place_stoppable_base<operation<MultiplexerHandle, Receiver>>
	{
		using stoppable_base = ex::__in_place_stoppable_base<operation<MultiplexerHandle, Receiver>>;

		using handle_type = HandleTemplate<MultiplexerHandle>;
		using multiplexer_type = typename MultiplexerHandle::multiplexer_type;
		using operation_type = async_operation_t<multiplexer_type, Object, Operation>;
		using io_status_type = typename multiplexer_type::io_status_type;
		using io_handler_type = io_handler<multiplexer_type>;

		handle_type m_handle;
		vsm_no_unique_address params_type m_args;
		vsm_no_unique_address operation_type m_operation;
		vsm_no_unique_address Receiver m_receiver;

	public:
		explicit operation(auto&& sender, auto&& receiver)
			: m_handle(get_multiplexer(ex::get_env(receiver)))
			, m_args(vsm_forward(sender).m_args)
			, m_receiver(vsm_forward(receiver))
		{
		}

	private:
		friend void tag_invoke(ex::start_t, operation& self) noexcept
		{
			auto r = submit_io(
				self.m_handle,
				self.m_operation,
				vsm_as_const(self.m_args),
				static_cast<io_handler_type&>(self));

			if (r.is_pending())
			{
				// This is not a stream sender.
				vsm_assert(!r.has_value());

				self.stoppable_base::register_stoppable(
					ex::get_stop_token(ex::get_env(self.m_receiver)));
			}
			else
			{
				self.handle_result(vsm_move(r));
			}
		}

		void notify(io_status_type&& status) noexcept
		{
			auto r = notify_io(
				m_handle,
				m_operation,
				vsm_as_const(m_args),
				vsm_move(status));

			// This is not a stream sender.
			vsm_assert(!r.is_pending());

			stoppable_base::deregister_stoppable();

			handle_result(vsm_move(r));
		}

		void handle_result(io_result<void> const& r)
		{
			if (r.has_value())
			{
				ex::set_value(vsm_move(m_receiver), vsm_move(m_handle));
			}
			else if (r.is_cancelled())
			{
				ex::set_stopped(vsm_move(m_receiver));
			}
			else
			{
				ex::set_error(vsm_move(m_receiver), r.error());
			}
		}

		void on_stop_requested()
		{
			cancel_io(m_handle, m_operation);
		}

		friend io_handler_base<multiplexer_type, operation<MultiplexerHandle, Receiver>>;
		friend stoppable_base;
	};

	vsm_no_unique_address params_type m_args;

public:
	using is_sender = void;

	template<typename E>
	friend auto tag_invoke(ex::get_completion_signatures_t, io_handle_sender const&, E&&)
		-> ex::completion_signatures<
			ex::set_value_t(HandleTemplate<environment_multiplexer_handle_t<E>>),
			ex::set_error_t(std::error_code),
			ex::set_stopped_t()
		>;

	template<std::convertible_to<params_type> Args>
	explicit io_handle_sender(Args&& args)
		: m_args(vsm_forward(args))
	{
	}

	template<ex::receiver Receiver>
	friend operation<current_multiplexer_t<Receiver>, std::decay_t<Receiver>> tag_invoke(
		ex::connect_t,
		vsm::any_cvref_of<io_handle_sender> auto&& sender,
		Receiver&& receiver)
	{
		return operation<current_multiplexer_t<Receiver>, std::decay_t<Receiver>>(
			vsm_forward(sender),
			vsm_forward(receiver));
	}
};

} // namespace allio::detail
