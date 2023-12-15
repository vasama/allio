#include <allio/impl/win32/byte_io.hpp>

#include <allio/impl/win32/kernel.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

template<auto const& Syscall>
static vsm::result<size_t> do_byte_io(
	platform_object_t::native_type const& h,
	auto const& a)
{
	static constexpr bool is_random = requires { a.offset; };

	HANDLE const handle = unwrap_handle(h.platform_handle);

	LARGE_INTEGER offset_integer;
	LARGE_INTEGER* p_offset_integer = nullptr;

	if constexpr (is_random)
	{
		offset_integer.QuadPart = a.offset;
		p_offset_integer = &offset_integer;
	}

	size_t transferred = 0;
	for (auto buffer : a.buffers.buffers())
	{
		while (!buffer.empty())
		{
			IO_STATUS_BLOCK io_status_block = make_io_status_block();

			// The maximum transfer size is limited by the range of size_t transferred.
			size_t const max_transfer_size_dynamic = std::min(
				buffer.size(),
				std::numeric_limits<size_t>::max() - transferred);

			// The maximum transfer size is limited by the range of the ULONG Length parameter.
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
				p_offset_integer,
				/* Key: */ 0);

			if (status == STATUS_PENDING)
			{
				//TODO: Use stepped deadline.
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
			transferred += transfer_size;

			if constexpr (is_random)
			{
				//TODO: Handle integer overflow of the offset?
				offset_integer.QuadPart += transfer_size;
			}

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

vsm::result<size_t> win32::random_read(
	platform_object_t::native_type const& h,
	byte_io::random_parameters_t<std::byte> const& a)
{
	return do_byte_io<win32::NtReadFile>(h, a);
}

vsm::result<size_t> win32::random_write(
	platform_object_t::native_type const& h,
	byte_io::random_parameters_t<std::byte const> const& a)
{
	return do_byte_io<win32::NtWriteFile>(h, a);
}

vsm::result<size_t> win32::stream_read(
	platform_object_t::native_type const& h,
	byte_io::stream_parameters_t<std::byte> const& a)
{
	return do_byte_io<win32::NtReadFile>(h, a);
}

vsm::result<size_t> win32::stream_write(
	platform_object_t::native_type const& h,
	byte_io::stream_parameters_t<std::byte const> const& a)
{
	return do_byte_io<win32::NtWriteFile>(h, a);
}
