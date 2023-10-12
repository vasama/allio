#include <allio/win32/detail/iocp/listen_handle.hpp>

#if 0
#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/iocp/socket.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/wsa_ex.hpp>
#include <allio/win32/kernel_error.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = _listen_socket_handle;
using C = connector_t<M, H>;

using accept_t = _listen_socket_handle::accept_t;
using accept_s = operation_t<M, H, accept_t>;
using accept_r = basic_accept_result_ref<M>;

static io_result helper(vsm::result<bool> const r)
{
	if (r)
	{
		if (*r)
		{
			return std::error_code();
		}
		else
		{
			return std::nullopt;
		}
	}
	else
	{
		return r.error();
	}
}

static vsm::result<bool> handle_completion(
	accept_r const r,
	unique_wrapped_socket socket,
	posix::socket_address_union const& addr)
{
	//TODO: Set file completion notification modes

	vsm_verify(r.result.socket.set_native_handle(
	{
		{
			H::flags::not_null,
		},
		socket.get(),
	}));
	(void)socket.release();

	r.result.endpoint = addr.get_network_endpoint();

	return true;
}

io_result operation_impl<M, H, accept_t>::submit(M& m, H const& h, C const& c, accept_s& s, accept_r const r)
{
	return helper([&]() -> vsm::result<bool>
	{
		if (!h)
		{
			return vsm::unexpected(error::handle_is_null);
		}

		SOCKET const listen_socket = posix::unwrap_socket(h.get_platform_handle());
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
		vsm_try(completed, submit_socket_io(
			m,
			h,
			wsa_accept_ex,
			listen_socket,
			posix::unwrap_socket(s.socket.get()),
			addr_buffer,
			overlapped));

		if (completed)
		{
			return handle_completion(r, vsm_move(s.socket), addr_buffer.remote);
		}

		return false;
	}());
}

io_result operation_impl<M, H, accept_t>::notify(M& m, H const& h, C const& c, accept_s& s, accept_r const r, io_status const p_status)
{
	auto const& status = M::unwrap_io_status(p_status);
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return static_cast<kernel_error>(status.status);
	}

	auto& addr_buffer = *std::launder(reinterpret_cast<wsa_accept_address_buffer*>(s.address_storage));
	return helper(handle_completion(r, vsm_move(s.socket), addr_buffer.remote));
}

void operation_impl<M, H, accept_t>::cancel(M& m, H const& h, C const& c, S& s)
{
	cancel_socket_io(posix::unwrap_socket(h.get_platform_handle()), *s.overlapped);
}
#endif
