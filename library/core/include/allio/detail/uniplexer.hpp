#pragma once

#include <allio/detail/io.hpp>

#include <vsm/platform.h>

namespace allio::detail {

class uniplexer_handle;

class uniplexer
{
public:
	using multiplexer_concept = void;
	using handle_type = uniplexer_handle;

	struct connector_type {};
	struct operation_type {};
	struct io_status_type {};
};

class uniplexer_handle
{
	template<typename Object>
	using handle_type = basic_attached_handle<Object, uniplexer_handle>;

public:
	using multiplexer_handle_concept = void;
	using multiplexer_type = uniplexer;

	uniplexer_handle() = default;

	uniplexer_handle(uniplexer&)
	{
	}

	template<typename Object, operation_c Operation>
	static vsm::result<io_result_t<handle_type<Object>, Operation>> blocking_io(
		handle_const_t<Operation, native_handle<Object>>& h,
		io_parameters_t<Object, Operation> const& a)
	{
		async_connector_t<multiplexer_type, Object> c;
		async_operation_t<multiplexer_type, Object, Operation> s;
		auto r = submit_io(multiplexer, h, c, s, a, handler);

		vsm_assert(get_io_notify_status(r) == io_notify_status::completed);

		if (!r)
		{
			return vsm::unexpected(r.error());
		}

		if constexpr (std::is_void_v<io_result_t<handle_type<Object>, Operation>>)
		{
			return {};
		}
		else
		{
			return rebind_handle<io_result_t<handle_type<Object>, Operation>>(vsm_move(*r));
		}
	}

	operator uniplexer& () const
	{
		return multiplexer;
	}

private:
	struct io_handler_type : io_handler_base<uniplexer, io_handler_type>
	{
		static void on_io_notification(uniplexer::io_status_type&&) noexcept
		{
			vsm_unreachable();
		}

		static void on_cancel_requested() noexcept
		{
			vsm_unreachable();
		}
	};

	static uniplexer multiplexer;
	static io_handler_type handler;
};
inline constinit uniplexer uniplexer_handle::multiplexer;
inline constinit uniplexer_handle::io_handler_type uniplexer_handle::handler;

template<std::same_as<uniplexer> Multiplexer, object Object>
struct async_connector<Multiplexer, Object> : uniplexer::connector_type
{
	static vsm::result<void> attach(Multiplexer&, native_handle<Object> const&, async_connector&)
	{
		return {};
	}

	static vsm::result<void> detach(Multiplexer&, native_handle<Object> const&, async_connector&)
	{
		return {};
	}
};

template<std::same_as<uniplexer> Multiplexer, object Object, operation_c Operation>
struct async_operation<Multiplexer, Object, Operation> : uniplexer::operation_type
{
	using M = Multiplexer;
	using H = handle_const_t<Operation, native_handle<Object>>;
	using C = handle_const_t<Operation, async_connector_t<M, Object>>;
	using S = async_operation;
	using A = io_parameters_t<Object, Operation>;
	using R = io_result_t<basic_attached_handle<Object, uniplexer_handle>, Operation>;

	static io_result<R> submit(M&, H& h, C&, S&, A const& a, io_handler<M>&)
	{
		auto r = blocking_io<Operation>(h, a);

		if (!r)
		{
			return vsm::unexpected(r.error());
		}

		if constexpr (std::is_void_v<io_result_t<basic_attached_handle<Object, uniplexer_handle>, Operation>>)
		{
			return {};
		}
		else
		{
			return rebind_handle<R>(
				vsm_move(*r),
				uniplexer_handle());
		}
	}

	static io_result<R> notify(M&, H&, C&, S&, A const&, M::io_status_type&&)
	{
		vsm_unreachable();
	}

	static void cancel(M&, H const&, C const&, S&)
	{
	}
};

} // namespace allio::detail
