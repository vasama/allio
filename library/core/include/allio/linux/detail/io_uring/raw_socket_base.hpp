#pragma once

#include <allio/detail/handles/socket_base.hpp>
#include <allio/linux/detail/io_uring/multiplexer.hpp>

namespace allio::detail {

struct io_uring_raw_socket_get_address_1
	: async_extension
	, io_uring_multiplexer::operation_type
{
	using M = io_uring_multiplexer;
	using H = native_handle<platform_object_t>;
	using C = async_connector_t<M, platform_object_t>;
	using S = io_uring_raw_socket_get_address_1;
	using A = get_address_t::params_type;

	unsigned addr_size;

	static io_result<void> submit(M& m, H const& h, C const& c, S& s, A const& a, io_handler<M>& handler);
	static io_result<void> notify(M& m, H const& h, C const& c, S& s, A const& a, io_handler<M>& handler, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s, A const& a, io_handler<M> const& handler);
};

template<typename RawSocket>
struct io_uring_raw_socket_get_address
	: io_uring_raw_socket_get_address_1
{
	using M = io_uring_multiplexer;
	using H = native_handle<RawSocket>;
	using C = async_connector_t<M, RawSocket>;
	using S = async_operation_t<M, RawSocket, get_address_t>;
	using A = io_parameters_t<RawSocket, get_address_t>;

	static vsm_always_inline io_result<void> submit(M& m, H const& h, C const& c, S& s, A const& a, io_handler<M>& handler)
	{
		return io_uring_raw_socket_get_address::submit(m, h, c, s, a, handler);
	}

	static vsm_always_inline io_result<void> notify(M& m, H const& h, C const& c, S& s, A const& a, io_handler<M>& handler, M::io_status_type status)
	{
		return io_uring_raw_socket_get_address::notify(m, h, c, s, a, handler, vsm_move(status));
	}

	static vsm_always_inline void cancel(M& m, H const& h, C const& c, S& s, A const& a, io_handler<M> const& handler)
	{
		return io_uring_raw_socket_get_address::cancel(m, h, c, s, a, handler);
	}
};

} // namespace allio::detail
