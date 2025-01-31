#include <allio/impl/win32/byte_io.hpp>

#include <allio/detail/byte_io_buffer_range.hpp>
#include <allio/detail/unique_handle.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/thread_event.hpp>
#include <allio/step_deadline.hpp>
#include <allio/win32/handles/platform_object.hpp>

#include <vsm/numeric.hpp>
#include <vsm/out_resource.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

static constexpr fs_size max_file_extent =
	static_cast<fs_size>(std::numeric_limits<LONGLONG>::max());

template<auto const& Syscall>
static vsm::result<size_t> do_byte_io(native_handle<platform_object_t> const& h, auto const& a)
{
	static constexpr bool is_random_access = requires { a.offset; };

	if (a.deadline != deadline::never() &&
		h.flags[platform_object_t::impl_type::flags::synchronous])
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	//TODO: Apply event based overlapped I/O to other synchronous I/O operations.
	vsm_try(event, thread_event::get_for(h));

	step_deadline absolute_deadline = a.deadline;

	HANDLE const handle = unwrap_handle(h.platform_handle);

	LARGE_INTEGER offset_integer;
	LARGE_INTEGER* p_offset_integer = nullptr;

	if constexpr (is_random_access)
	{
		vsm_try_assign(offset_integer.QuadPart, vsm::try_truncate<LONGLONG>(
			a.offset,
			error::file_offset_out_of_range));

		p_offset_integer = &offset_integer;
	}

	auto const transfer_some = [&](
		void const* const data,
		ULONG const max_transfer_size) -> vsm::result<size_t>
	{
		vsm_assert(max_transfer_size != 0);

		vsm_try(relative_deadline, absolute_deadline.step());

		thread_event::io_status_block_t io_status_block;
		NTSTATUS status = Syscall(
			handle,
			event,
			/* ApcRoutine: */ nullptr,
			/* ApcContext: */ nullptr,
			&io_status_block,
			// NtWriteFile takes void* which requires casting away the const.
			const_cast<void*>(data),
			max_transfer_size,
			p_offset_integer,
			/* Key: */ nullptr);

		if (status == STATUS_PENDING)
		{
			status = event.wait_for_io(
				handle,
				io_status_block,
				relative_deadline);
		}

		if (!is_kernel_success(status))
		{
			return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
		}

		if (io_status_block.Information == 0)
		{
			return vsm::unexpected(allio_error(error::end_of_stream));
		}

		return io_status_block.Information;
	};

	new_io_buffer_layout const layout = a.buffers.get_layout();

	size_t transferred = 0;
	for (new_io_buffer const io_buffer : read_io_buffers(a.buffers.get_buffers()))
	{
		auto buffer = get_io_buffer_span<std::byte const>(io_buffer, layout);

		while (!buffer.empty())
		{
			// The maximum transfer size is limited by the range of size_t transferred.
			size_t const max_transfer_size_dynamic = std::min(
				buffer.size(),
				std::numeric_limits<size_t>::max() - transferred);

			// The maximum transfer size is limited by the range of the ULONG Length parameter.
			ULONG const max_transfer_size = static_cast<ULONG>(std::min(
				max_transfer_size_dynamic,
				static_cast<size_t>(std::numeric_limits<ULONG>::max())));

			auto const r = transfer_some(buffer.data(), max_transfer_size);

			if (!r)
			{
				if (transferred != 0)
				{
					// The error is ignored if some data was already transferred.
					goto outer_break;
				}

				return vsm::unexpected(r.error());
			}

			size_t const transfer_size = *r;
			transferred += transfer_size;

			if constexpr (is_random_access)
			{
				//TODO: Does this make sense? The completed I/O would have had to go past the max.
				if (max_file_extent - transfer_size > static_cast<fs_size>(offset_integer.QuadPart))
				{
					goto outer_break;
				}

				offset_integer.QuadPart += static_cast<LONGLONG>(transfer_size);
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
	native_handle<platform_object_t> const& h,
	byte_io::random_parameters_t<std::byte> const& a)
{
	return do_byte_io<win32::NtReadFile>(h, a);
}

vsm::result<size_t> win32::random_write(
	native_handle<platform_object_t> const& h,
	byte_io::random_parameters_t<std::byte const> const& a)
{
	return do_byte_io<win32::NtWriteFile>(h, a);
}

vsm::result<size_t> win32::stream_read(
	native_handle<platform_object_t> const& h,
	byte_io::stream_parameters_t<std::byte> const& a)
{
	return do_byte_io<win32::NtReadFile>(h, a);
}

vsm::result<size_t> win32::stream_write(
	native_handle<platform_object_t> const& h,
	byte_io::stream_parameters_t<std::byte const> const& a)
{
	return do_byte_io<win32::NtWriteFile>(h, a);
}
