#pragma once

#include <allio/async_fwd.hpp>
#include <allio/byte_io.hpp>
#include <allio/platform_handle.hpp>

namespace allio {
namespace detail {

class usb_stream_handle_base : public platform_handle
{
	using final_handle_type = final_handle<usb_stream_handle_base>;

public:
	using base_type = platform_handle;

	using async_operations = type_list_cat<
		base_type::async_operations,
		io::stream_scatter_gather
	>;


	vsm::result<void> read(read_buffers buffers, deadline deadline = {});
	vsm::result<void> read(read_buffer const buffer, deadline const deadline = {})
	{
		return read(read_buffers(&buffer, 1), deadline);
	}

	vsm::result<void> write(write_buffers buffers, deadline deadline = {});
	vsm::result<void> write(write_buffer const buffer, deadline const deadline = {})
	{
		return write(write_buffers(&buffer, 1), deadline);
	}

	basic_sender<io::stream_scatter_read> read_async(read_buffers buffers, deadline deadline = {});
	basic_sender<io::stream_scatter_read> read_async(read_buffer buffer, deadline deadline = {});

	basic_sender<io::stream_gather_write> write_async(write_buffers buffers, deadline deadline = {});
	basic_sender<io::stream_gather_write> write_async(write_buffer buffer, deadline deadline = {});

protected:
	using base_type::base_type;
};

} // namespace detail

using usb_stream_handle = final_handle<detail::usb_stream_handle_base>;



allio_USB_API extern allio_handle_implementation(usb_stream_handle);

} // namespace allio
