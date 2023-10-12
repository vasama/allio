#pragma once

#include <allio/detail/handles/listen_handle.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/detail/unique_socket.hpp>

namespace allio::detail {

template<>
struct operation_impl<iocp_multiplexer, raw_listen_handle_t, raw_listen_handle_t::accept_t>
{
	using M = iocp_multiplexer;
	using H = raw_listen_handle_t;
	using O = raw_listen_handle_t::accept_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using R = basic_accept_result_ref<basic_raw_socket_handle<basic_multiplexer_handle<M>>>;

	unique_wrapped_socket socket;
	iocp_multiplexer::overlapped overlapped;
	std::byte address_storage[256];

	static io_result submit(M& m, H const& h, C const& c, S& s, R r);
	static io_result notify(M& m, H const& h, C const& c, S& s, R r, io_status status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
