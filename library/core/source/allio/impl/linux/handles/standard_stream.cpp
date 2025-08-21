#include <allio/detail/handles/standard_stream.hpp>

#include <allio/impl/linux/byte_io.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

static int get_std_fd(native_handle<standard_stream_t> const& h)
{
	if (h.flags[standard_stream_t::flags::input_stream])
	{
		return STDIN_FILENO;
	}
	else
	{
		if (h.flags[standard_stream_t::flags::error_stream])
		{
			return STDERR_FILENO;
		}
		else
		{
			return STDOUT_FILENO;
		}
	}
}

static native_handle<platform_object_t> make_std_platform_handle(
	native_handle<standard_stream_t> const& h)
{
	return native_handle<platform_object_t>
	{
		native_handle<object_t>
		{
			.flags = handle_flags(object_t::flags::not_null)
		},
		wrap_handle(get_std_fd(h)),
	};
}

vsm::result<size_t> standard_stream_t::stream_read(
	native_handle<standard_stream_t> const& h,
	io_parameters_t<standard_stream_t, stream_read_t> const& args)
{
	return linux::stream_read(make_std_platform_handle(h), args);
}

vsm::result<size_t> standard_stream_t::stream_write(
	native_handle<standard_stream_t> const& h,
	io_parameters_t<standard_stream_t, stream_write_t> const& args)
{
	return linux::stream_write(make_std_platform_handle(h), args);
}
