#include <allio/detail/handles/standard_stream.hpp>

#include <allio/impl/error_encoding.hpp>
#include <allio/impl/win32/byte_io.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/handles/standard_stream.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

static vsm::result<void> standard_stream_t::stream_read(
	native_handle<standard_stream_t> const& h,
	io_parameters_t<standard_stream_t, stream_read_t> const& args)
{
	if (h.m_flags[impl_type::flags::console])
	{
		
	}
	else
	{
		native_handle<platform_object_t> const local_h =
		{
			native_handle<object_t>
			{
				h.flags,
			},

			h.platform_handle,
		};

		return win32::stream_read(local_h, args);
	}
}

static vsm::result<void> standard_stream_t::stream_write(
	native_handle<standard_stream_t> const& h,
	io_parameters_t<standard_stream_t, stream_write_t> const& args)
{
	if (h.m_flags[impl_type::flags::console])
	{
		
	}
	else
	{
		native_handle<platform_object_t> const local_h =
		{
			native_handle<object_t>
			{
				h.flags,
			},

			h.platform_handle,
		};

		return win32::stream_write(local_h, args);
	}
}
