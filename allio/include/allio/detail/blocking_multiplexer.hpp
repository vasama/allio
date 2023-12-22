#pragma once

#include <allio/detail/io.hpp>

namespace allio::detail {

class blocking_multiplexer
{
public:
	using multiplexer_concept = void;
	using connector_type = async_connector_base;
	using operation_type = async_operation_base;
	struct io_status_type {};
};

class blocking_multiplexer_handle
{
public:
	using multiplexer_handle_concept = void;
	using multiplexer_type = blocking_multiplexer;

	template<object Object, operation_c Operation>
	static vsm::result<io_result_t<Object, Operation>> blocking_io(
		handle_const_t<Operation, typename Object::native_type>& h,
		io_parameters_t<Object, Operation> const& a)
	{
		async_connector_t<multiplexer_type, Object> c;
		async_operation_t<multiplexer_type, Object, Operation> s;
		auto r = submit_io(multiplexer, h, c, s, a, handler);
		vsm_assert(!r.is_pending() && !r.is_cancelled());

		if (!r)
		{
			return vsm::unexpected(r.error());
		}

		if constexpr (std::is_void_v<io_result_t<Object, Operation>>)
		{
			return {};
		}
		else
		{
			return rebind_handle<io_result_t<Object, Operation>>(vsm_move(*r));
		}
	}

	operator blocking_multiplexer& () const
	{
		return multiplexer;
	}

private:
	static void io_callback(
		io_handler<blocking_multiplexer>&,
		blocking_multiplexer::io_status_type&&) noexcept
	{
		vsm_unreachable();
	}

	static blocking_multiplexer multiplexer;
	static io_handler<blocking_multiplexer> handler;
};
inline blocking_multiplexer blocking_multiplexer_handle::multiplexer;
inline io_handler<blocking_multiplexer> blocking_multiplexer_handle::handler(io_callback);

template<std::same_as<blocking_multiplexer> Multiplexer, object Object>
struct async_connector<Multiplexer, Object> : blocking_multiplexer::connector_type
{
	using M = Multiplexer;
	using H = typename Object::native_type const;
	using C = async_connector;

	static vsm::result<void> attach(M&, H&, C&)
	{
		return {};
	}

	static vsm::result<void> detach(M&, H&, C&)
	{
		return {};
	}
};

template<std::same_as<blocking_multiplexer> Multiplexer, object Object, operation_c Operation>
struct async_operation<Multiplexer, Object, Operation> : blocking_multiplexer::operation_type
{
	using M = Multiplexer;
	using H = handle_const_t<Operation, typename Object::native_type>;
	using C = handle_const_t<Operation, async_connector_t<M, Object>>;
	using S = async_operation;
	using A = io_parameters_t<Object, Operation>;
	using R = io_result_t<Object, Operation, blocking_multiplexer_handle>;

	static io_result<R> submit(M&, H& h, C&, S&, A const& a, io_handler<M>&)
	{
		auto r = blocking_io<Operation>(h, a);

		if (!r)
		{
			return vsm::unexpected(r.error());
		}

		if constexpr (std::is_void_v<io_result_t<Object, Operation>>)
		{
			return {};
		}
		else
		{
			return rebind_handle<R>(
				vsm_move(*r),
				blocking_multiplexer_handle());
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
