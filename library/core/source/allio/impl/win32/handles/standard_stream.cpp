#include <allio/detail/handles/standard_stream.hpp>

#include <allio/impl/win32/byte_io.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/handles/platform_object.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

static DWORD get_std_handle_key(native_handle<standard_stream_t> const& h)
{
	if (h.flags[standard_stream_t::flags::stream_bit_1])
	{
		return STD_ERROR_HANDLE;
	}
	
	if (h.flags[standard_stream_t::flags::stream_bit_0])
	{
		return STD_OUTPUT_HANDLE;
	}

	return STD_INPUT_HANDLE;
}

static HANDLE get_std_handle(native_handle<standard_stream_t> const& h)
{
	return GetStdHandle(get_std_handle_key(h));
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
		wrap_handle(get_std_handle(h)),
	};
}

vsm::result<size_t> standard_stream_t::stream_read(
	native_handle<standard_stream_t> const& h,
	io_parameters_t<standard_stream_t, stream_read_t> const& args)
{
	return win32::stream_read(make_std_platform_handle(h), args);
}

vsm::result<size_t> standard_stream_t::stream_write(
	native_handle<standard_stream_t> const& h,
	io_parameters_t<standard_stream_t, stream_write_t> const& args)
{
	return win32::stream_write(make_std_platform_handle(h), args);
}
