#include <allio/impl/posix/socket.hpp>

#include <allio/impl/linux/timeout.hpp>
#include <allio/impl/linux/byte_io.hpp>

#include <vsm/lazy.hpp>
#include <vsm/numeric.hpp>

#include <fcntl.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;
using namespace allio::linux;

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
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}

	return vsm_lazy(socket_with_flags
	{
		.socket = unique_socket(socket),
	});
}

vsm::result<socket_with_flags> posix::socket_accept(
	socket_type const listen_socket,
	sockaddr* const addr,
	socket_address_size_type* const addr_size,
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

	socket_type const socket = accept4(
		listen_socket,
		addr,
		addr_size,
		accept_flags);

	if (socket == socket_error_value)
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
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
		return vsm::unexpected(allio_error(get_last_socket_error()));
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
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}

	if ((old_flags & O_NONBLOCK) == new_flags)
	{
		return {};
	}

	if (fcntl(socket, F_SETFL, (old_flags & ~O_NONBLOCK) | new_flags) == -1)
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}

	return {};
}

//TODO: Detect the iovec layout automatically.
static constexpr auto layout = new_io_buffer_layout::data_size;

vsm::result<size_t> posix::socket_scatter_read(
	socket_type const socket,
	new_read_buffers const buffers)
{
	dynamic_io_vector_storage storage;
	vsm_try(transformed_buffers, get_io_buffers(buffers, layout, storage));

	ssize_t const r = readv(
		socket,
		reinterpret_cast<iovec const*>(transformed_buffers.buffers_data),
		vsm::saturating(transformed_buffers.buffers_size));

	if (r == -1)
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}

	if (r == 0 && !io_buffers_is_empty(buffers))
	{
		return vsm::unexpected(allio_error(error::end_of_stream));
	}

	return static_cast<size_t>(r);
}

vsm::result<size_t> posix::socket_gather_write(
	socket_type const socket,
	new_write_buffers const buffers)
{
	dynamic_io_vector_storage storage;
	vsm_try(transformed_buffers, get_io_buffers(buffers, layout, storage));

	ssize_t const r = writev(
		socket,
		reinterpret_cast<iovec const*>(transformed_buffers.buffers_data),
		vsm::saturating(transformed_buffers.buffers_size));

	if (r == -1)
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}

	return static_cast<size_t>(r);
}

vsm::result<size_t> posix::socket_receive_from(
	socket_type const socket,
	sockaddr* const addr,
	socket_address_size_type* const addr_size,
	new_read_buffers const buffers)
{
	dynamic_io_vector_storage storage;
	vsm_try(transformed_buffers, get_io_buffers(buffers, layout, storage));
	auto const vectors = reinterpret_cast<iovec const*>(transformed_buffers.buffers_data);

	msghdr message =
	{
		.msg_name = addr,
		.msg_namelen = *addr_size,
		// msghdr::msg_iov seems to be non-const-correct.
		.msg_iov = const_cast<iovec*>(vectors),
		.msg_iovlen = transformed_buffers.buffers_size,
	};

	ssize_t const r = recvmsg(
		socket,
		&message,
		/* flags: */ 0);

	if (r == -1)
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}

	*addr_size = message.msg_namelen;
	return static_cast<size_t>(r);
}

vsm::result<void> posix::socket_send_to(
	socket_type const socket,
	socket_address_view const addr,
	new_write_buffers const buffers)
{
	dynamic_io_vector_storage storage;
	vsm_try(transformed_buffers, get_io_buffers(buffers, layout, storage));
	auto const vectors = reinterpret_cast<iovec const*>(transformed_buffers.buffers_data);

	msghdr const message =
	{
		.msg_name = const_cast<sockaddr*>(addr.addr),
		.msg_namelen = addr.size,
		// msghdr::msg_iov seems to be non-const-correct.
		.msg_iov = const_cast<iovec*>(vectors),
		.msg_iovlen = transformed_buffers.buffers_size,
	};

	ssize_t const r = sendmsg(
		socket,
		&message,
		/* flags: */ 0);

	if (r == -1)
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}

	// The transferred size must match the total specified in the buffers.
	vsm_assert_slow(static_cast<size_t>(r) == get_io_buffers_size(buffers));

	return {};
}
