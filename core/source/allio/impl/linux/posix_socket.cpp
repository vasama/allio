#include <allio/impl/posix/socket.hpp>

#include <allio/impl/linux/timeout.hpp>

#include <vsm/lazy.hpp>
#include <vsm/numeric.hpp>

#include <fcntl.h>
#include <sys/uio.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

vsm::result<socket_with_flags> posix::create_socket(
	int const address_family,
	int type,
	int const protocol,
	io_flags const flags)
{
	if (vsm::no_flags(flags, io_flags::create_inheritable))
	{
		type |= SOCK_CLOEXEC;
	}

	socket_type const socket = ::socket(
		address_family,
		type,
		protocol);

	if (socket == invalid_socket)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	return vsm_lazy(socket_with_flags
	{
		.socket = unique_socket(socket),
	});
}

vsm::result<socket_with_flags> posix::socket_accept(
	socket_type const listen_socket,
	socket_address& addr,
	deadline const deadline,
	io_flags const flags)
{
	if (deadline != deadline::never())
	{
		vsm_try_void(socket_poll_or_timeout(listen_socket, socket_poll_r, deadline));
	}

	int accept_flags = 0;

	if (vsm::no_flags(flags, io_flags::create_inheritable))
	{
		accept_flags |= SOCK_CLOEXEC;
	}

	if (vsm::any_flags(flags, io_flags::create_non_blocking))
	{
		accept_flags |= SOCK_NONBLOCK;
	}

	addr.size = sizeof(socket_address_union);
	socket_type const socket = accept4(
		listen_socket,
		&addr.addr,
		&addr.size,
		accept_flags);

	if (socket == socket_error_value)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	return vsm_lazy(socket_with_flags
	{
		.socket = unique_socket(socket),
	});
}

vsm::result<posix::socket_poll_mask> posix::socket_poll(
	socket_type const socket,
	socket_poll_mask const mask,
	deadline const deadline)
{
	pollfd poll_fd =
	{
		.fd = socket,
		.events = mask,
	};

	int const r = ppoll(
		&poll_fd,
		/* fds: */ 1,
		kernel_timeout<timespec>(deadline),
		/* sigmask: */ nullptr);

	if (r == -1)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	if (r == 0)
	{
		return 0;
	}

	vsm_assert(poll_fd.revents & mask);
	return poll_fd.revents;
}

vsm::result<void> posix::socket_set_non_blocking(
	socket_type const socket,
	bool const non_blocking)
{
	int const new_flags = non_blocking ? O_NONBLOCK : 0;
	int const old_flags = fcntl(socket, F_GETFL);

	if (old_flags == -1)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	if ((old_flags & O_NONBLOCK) == new_flags)
	{
		return {};
	}

	if (fcntl(socket, F_SETFL, (old_flags & ~O_NONBLOCK) | new_flags) == -1)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	return {};
}

vsm::result<size_t> posix::socket_scatter_read(
	socket_type const socket,
	read_buffers const buffers)
{
	ssize_t const r = readv(
		socket,
		reinterpret_cast<iovec const*>(buffers.data()),
		vsm::saturating(buffers.size()));

	if (r == -1)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	return static_cast<size_t>(r);
}

vsm::result<size_t> posix::socket_gather_write(
	socket_type const socket,
	write_buffers const buffers)
{
	ssize_t const r = writev(
		socket,
		reinterpret_cast<iovec const*>(buffers.data()),
		vsm::saturating(buffers.size()));

	if (r == -1)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	return static_cast<size_t>(r);
}

vsm::result<size_t> posix::socket_receive_from(
	socket_type const socket,
	socket_address& addr,
	read_buffers const buffers)
{
	msghdr message =
	{
		.msg_name = &addr.addr,
		.msg_namelen = sizeof(socket_address_union),
		// msghdr::msg_iov seems to be non-const-correct.
		.msg_iov = reinterpret_cast<iovec*>(const_cast<read_buffer*>(buffers.data())),
		.msg_iovlen = buffers.size(),
	};

	ssize_t const r = recvmsg(
		socket,
		&message,
		/* flags: */ 0);

	if (r == -1)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	addr.size = message.msg_namelen;
	return static_cast<size_t>(r);
}

vsm::result<void> posix::socket_send_to(
	socket_type const socket,
	socket_address const& addr,
	write_buffers const buffers)
{
	msghdr const message =
	{
		.msg_name = &const_cast<socket_address&>(addr).addr,
		.msg_namelen = addr.size,
		// msghdr::msg_iov seems to be non-const-correct.
		.msg_iov = reinterpret_cast<iovec*>(const_cast<write_buffer*>(buffers.data())),
		.msg_iovlen = buffers.size(),
	};

	ssize_t const r = sendmsg(
		socket,
		&message,
		/* flags: */ 0);

	if (r == -1)
	{
		return vsm::unexpected(get_last_socket_error());
	}
	//TODO: Assert transferred against total buffers size.

	return {};
}
