#pragma once

#include <allio/linux/detail/io_uring/multiplexer.hpp>

namespace allio::detail {

struct io_uring_byte_io_state
{
	using M = io_uring_multiplexer;
	using H = platform_object_t::native_type const;
	using C = async_connector_t<M, platform_object_t> const;
	using S = M::operation_type;

	M::timeout timeout;

	io_result<size_t> submit(M& m, H& h, C& c, S& s, io_handler<M>& handler);
	io_result<size_t> notify(M& m, H& h, C& c, S& s, M::io_status_type status);
	void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
