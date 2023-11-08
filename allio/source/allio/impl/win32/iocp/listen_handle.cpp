#include <allio/win32/detail/iocp/listen_handle.hpp>

#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/iocp/socket.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/wsa_ex.hpp>
#include <allio/win32/kernel_error.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = raw_listen_handle_t;
using N = H::native_type;
using C = connector_t<M, H>;

using accept_t = H::accept_t;
using accept_s = operation_t<M, H, accept_t>;
using accept_r = accept_result<async_handle<H, basic_multiplexer_handle<M>>>;

static io_result2<accept_r> make_accept_result(
	M& multiplexer,
	unique_wrapped_socket socket,
	posix::socket_address_union const& addr)
{
	//TODO: Set file completion notification modes

	return vsm_lazy(accept_r
	{
		async_handle<H, basic_multiplexer_handle<M>>(
			adopt_handle_t(),
			vsm_lazy(platform_handle_t::native_type
			{
				{
					H::flags::not_null,
				},
				socket.release(),
			}),
			multiplexer,
			vsm_lazy(C
			{
			})),
		addr.get_network_endpoint(),
	});
}

io_result2<accept_r> operation_impl<M, H, accept_t>::submit(M& m, N const& h, C const& c, accept_s& s)
{
	SOCKET const listen_socket = posix::unwrap_socket(h.platform_handle);
	//TODO: Cache the address family.
	vsm_try(addr, posix::socket_address::get(listen_socket));
	vsm_try(socket, posix::create_socket(addr.addr.sa_family));
	s.socket = unique_wrapped_socket(posix::wrap_socket(socket.socket.release()));

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	static_assert(sizeof(s.address_storage) >= sizeof(wsa_accept_address_buffer));
	auto& addr_buffer = *new (s.address_storage) wsa_accept_address_buffer;

	// If using a multithreaded completion port, after this call
	// another thread will race to complete this operation.
	vsm_try(already_connected, submit_socket_io(
		m,
		h,
		[&]()
		{
			return wsa_accept_ex(
				listen_socket,
				posix::unwrap_socket(s.socket.get()),
				addr_buffer,
				overlapped);
		}));

	if (already_connected)
	{
		return make_accept_result(m, vsm_move(s.socket), addr_buffer.remote);
	}

	return io_pending;
}

io_result2<accept_r> operation_impl<M, H, accept_t>::notify(M& m, N const& h, C const& c, accept_s& s, io_status const p_status)
{
	auto const& status = M::unwrap_io_status(p_status);
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	auto& addr_buffer = *std::launder(reinterpret_cast<wsa_accept_address_buffer*>(s.address_storage));
	return make_accept_result(m, vsm_move(s.socket), addr_buffer.remote);
}

void operation_impl<M, H, accept_t>::cancel(M& m, N const& h, C const& c, S& s)
{
	cancel_socket_io(posix::unwrap_socket(h.platform_handle), *s.overlapped);
}
