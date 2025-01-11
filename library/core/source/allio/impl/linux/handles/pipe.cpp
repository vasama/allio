#include <allio/detail/handles/pipe.hpp>

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/linux/byte_io.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/fcntl.hpp>

#include <vsm/lazy.hpp>

#include <unistd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

using pipe_pair = basic_pipe_pair<unique_handle>;

static vsm::result<pipe_pair> _pipe(int const flags)
{
	int fd[2];
	if (pipe2(fd, flags) == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm_lazy(pipe_pair
	{
		.read_pipe = unique_handle(fd[0]),
		.write_pipe = unique_handle(fd[1]),
	});
}

vsm::result<void> pipe_pair_t::create_pair(
	native_handle<pipe_pair_t>& h,
	io_parameters_t<pipe_pair_t, create_pair_t> const& a)
{
	int pipe_flags = 0;

	bool const r_inheritable = vsm::any_flags(a.read_pipe.flags, io_flags::create_inheritable);
	bool const w_inheritable = vsm::any_flags(a.write_pipe.flags, io_flags::create_inheritable);

	if (!r_inheritable || !w_inheritable)
	{
		// If either handle is not inheritable, apply CLOEXEC to both on open. If one is
		// inheritable, the CLOEXEC is removed from it after the pipe has been created.
		pipe_flags |= O_CLOEXEC;
	}

	vsm_try(pipes, _pipe(pipe_flags));

	if (r_inheritable && !w_inheritable)
	{
		vsm_try_void(linux::set_inheritable(pipes.read_pipe.get(), /* inheritable: */ true));
	}
	if (w_inheritable && !r_inheritable)
	{
		vsm_try_void(linux::set_inheritable(pipes.write_pipe.get(), /* inheritable: */ true));
	}

	h.flags = flags::not_null;
	h.r_h.flags = flags::not_null;
	h.r_h.platform_handle = wrap_handle(pipes.read_pipe.release());
	h.w_h.flags = flags::not_null;
	h.w_h.platform_handle = wrap_handle(pipes.write_pipe.release());

	return {};
}

vsm::result<size_t> pipe_t::stream_read(
	native_handle<pipe_t> const& h,
	io_parameters_t<pipe_t, stream_read_t> const& a)
{
	return linux::stream_read(h, a);
}

vsm::result<size_t> pipe_t::stream_write(
	native_handle<pipe_t> const& h,
	io_parameters_t<pipe_t, stream_write_t> const& a)
{
	return linux::stream_write(h, a);
}
