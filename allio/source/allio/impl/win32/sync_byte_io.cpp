#include <allio/impl/sync_byte_io.hpp>

#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/nt_error.hpp>
#include <allio/win32/platform.hpp>

using namespace allio;
using namespace allio::win32;

static vsm::result<size_t> sync_scatter_gather_io(platform_handle const& h, io::scatter_gather_parameters const& args, decltype(win32::NtReadFile) const syscall)
{
	if (!h)
	{
		return std::unexpected(error::handle_is_null);
	}

	vsm_try_void(kernel_init());

	HANDLE const handle = unwrap_handle(h.get_platform_handle());
	file_size offset = args.offset;

	size_t transferred = 0;
	for (auto const buffer : args.buffers.buffers())
	{
		LARGE_INTEGER offset_integer;
		offset_integer.QuadPart = args.offset;

		IO_STATUS_BLOCK io_status_block = make_io_status_block();

		NTSTATUS status = syscall(
			handle,
			NULL,
			nullptr,
			nullptr,
			&io_status_block,
			(PVOID)buffer.data(),
			buffer.size(),
			&offset_integer,
			0);

		if (status == STATUS_PENDING)
		{
			status = io_wait(handle, &io_status_block, args.deadline);
		}

		if (!NT_SUCCESS(status))
		{
			if (transferred != 0)
			{
				break;
			}

			return std::unexpected(static_cast<nt_error>(status));
		}

		transferred += io_status_block.Information;

		if (io_status_block.Information != buffer.size())
		{
			break;
		}

		offset += buffer.size();
	}
	return transferred;
}

vsm::result<size_t> allio::sync_scatter_read(platform_handle const& h, io::scatter_gather_parameters const& args)
{
	return sync_scatter_gather_io(h, args, win32::NtReadFile);
}

vsm::result<size_t> allio::sync_gather_write(platform_handle const& h, io::scatter_gather_parameters const& args)
{
	return sync_scatter_gather_io(h, args, win32::NtWriteFile);
}
