#include <allio/usb/usb_cdc_handle.hpp>
#include <allio/win32/iocp_multiplexer.hpp>

#include <allio/implementation.hpp>

#include <winusb.h>

using namespace allio;
using namespace allio::win32;

template<>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, usb_cdc_handle, io::stream_scatter_read>
{
	static constexpr bool block_synchronous = true;

	struct async_operation_storage : basic_async_operation_storage<io::stream_scatter_read>
	{
		iocp_multiplexer::overlapped slot;
	};

	static vsm::result<void> start(iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> async_result<void>
		{
			usb_cdc_handle const& h = *s.args.handle;

			if (!h)
			{

			}

			read_buffers const buffers = s.args.buffers;

			OVERLAPPED& overlapped = *s.slot;
			overlapped.Pointer = nullptr;
			overlapped.hEvent = NULL;

			for (read_buffer const buffer : buffers)
			{
				if (WinUsb_ReadPipe(
					unwrap_usb(s.arguments->handle->get_native_handle()),
					0,
					reinterpret_cast<PUCHAR*>(buffer.data()),
					buffer.size(),
					nullptr,
					&overlapped))
				{
					
				}
				else if (DWORD const e = GetLastError(); e != ERROR_IO_PENDING)
				{

				}
			}
		});
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, usb_cdc_handle, io::stream_gather_write>
{
	using async_operation_storage = iocp_multiplexer::basic_async_operation_storage<io::socket_listen>;

	static vsm::result<void> start(iocp_multiplexer& m, async_operation_storage& s, bool const submit)
	{
	}
};
