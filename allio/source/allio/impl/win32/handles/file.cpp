#include <allio/detail/handles/file.hpp>

#include <allio/impl/win32/handles/fs_object.hpp>
#include <allio/impl/win32/kernel.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

template<auto const& Syscall, vsm::any_cv_of<std::byte> T>
static vsm::result<size_t> random_byte_io(
	platform_object_t::native_type const& h,
	byte_io::random_parameters_t<T> const& a)
{
	HANDLE const handle = unwrap_handle(h.platform_handle);

	fs_size offset = a.offset;
	size_t transferred = 0;

	for (auto buffer : a.buffers.buffers())
	{
		while (!buffer.empty())
		{
			IO_STATUS_BLOCK io_status_block = make_io_status_block();

			LARGE_INTEGER offset_integer;
			offset_integer.QuadPart = offset;

			// The maximum transfer size is limited by the range of size_t.
			size_t const max_transfer_size_dynamic = std::min(
				buffer.size(),
				std::numeric_limits<size_t>::max() - transferred);

			// The maximum transfer size is limited by the range of ULONG.
			ULONG const max_transfer_size = static_cast<ULONG>(std::min(
				max_transfer_size_dynamic,
				static_cast<size_t>(std::numeric_limits<ULONG>::max())));

			NTSTATUS status = Syscall(
				handle,
				/* Event: */ NULL,
				/* ApcRoutine: */ nullptr,
				/* ApcContext: */ nullptr,
				&io_status_block,
				// NtWriteFile takes void* which requires casting away the const.
				const_cast<std::byte*>(buffer.data()),
				max_transfer_size,
				&offset_integer,
				/* Key: */ 0);

			if (status == STATUS_PENDING)
			{
				status = io_wait(handle, &io_status_block, a.deadline);
			}

			if (!NT_SUCCESS(status))
			{
				if (transferred != 0)
				{
					// The error is ignored if some data was already transferred.
					goto outer_break;
				}

				return vsm::unexpected(static_cast<kernel_error>(status));
			}

			size_t const transfer_size = io_status_block.Information;

			offset += transfer_size;
			transferred += transfer_size;

			buffer = buffer.subspan(transfer_size);

			if (transfer_size != max_transfer_size)
			{
				// Stop looping if the transferred amount is less than requested.
				goto outer_break;
			}
		}
	}

outer_break:
	return transferred;
}

vsm::result<void> file_t::open(
	native_type& h,
	io_parameters_t<file_t, open_t> const& a)
{
	vsm_try_bind((file, flags), create_file(
		unwrap_handle(a.path.base),
		a.path.path,
		open_kind::file,
		a));

	h = platform_object_t::native_type
	{
		object_t::native_type
		{
			flags::not_null | flags,
		},
		wrap_handle(file.release()),
	};

	return {};
}

vsm::result<size_t> file_t::random_read(
	native_type const& h,
	io_parameters_t<file_t, random_read_t> const& a)
{
	return random_byte_io<win32::NtReadFile>(h, a);
}

vsm::result<size_t> file_t::random_write(
	native_type const& h,
	io_parameters_t<file_t, random_write_t> const& a)
{
	return random_byte_io<win32::NtWriteFile>(h, a);
}
