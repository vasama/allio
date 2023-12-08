#include <allio/detail/handles/pipe.hpp>

#include <allio/impl/linux/byte_io.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/linux/detail/unique_fd.hpp>
#include <allio/linux/platform.hpp>

#include <unistd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

using pipe_pair = basic_pipe_pair<unique_fd>;

static vsm::result<pipe_pair> _pipe(int const flags)
{
	int fd[2];
	if (pipe2(fd, flags) == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm_lazy(pipe_pair
	{
		.read_pipe = unique_fd(fd[0]),
		.write_pipe = unique_fd(fd[1]),
	});
}

vsm::result<void> pipe_t::create(native_type& r_h, native_type& w_h)
{
	int pipe_flags = 0;

	//TODO: Make pipe_io::create_t a proper I/O operation somehow.
	//      Decide how to handle an operation acting on two handles.
//	if (!a.inheritable)
//	{
//		pipe_flags |= O_CLOEXEC;
//	}

	vsm_try(pipe, _pipe(pipe_flags));

	r_h = platform_object_t::native_type
	{
		object_t::native_type
		{
			flags::not_null,
		},
		wrap_handle(pipe.read_pipe.release()),
	};

	w_h = platform_object_t::native_type
	{
		object_t::native_type
		{
			flags::not_null,
		},
		wrap_handle(pipe.write_pipe.release()),
	};

	return {};
}

vsm::result<size_t> pipe_t::stream_read(
	native_type const& h,
	io_parameters_t<pipe_t, read_some_t> const& a)
{
	return linux::stream_read(h, a);
}

vsm::result<size_t> pipe_t::stream_write(
	native_type const& h,
	io_parameters_t<pipe_t, write_some_t> const& a)
{
	return linux::stream_write(h, a);
}
