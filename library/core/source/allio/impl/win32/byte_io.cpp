#include <allio/impl/win32/byte_io.hpp>

#include <allio/detail/byte_io_buffer_range.hpp>
#include <allio/detail/byte_io_state.hpp>
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

namespace {

static constexpr fs_size max_file_extent =
	static_cast<fs_size>(std::numeric_limits<LONGLONG>::max());

template<vsm::any_cv_of<std::byte> T>
static auto is_read_buffer(io_buffers<T> const&) -> std::bool_constant<vsm::non_cv<T>>;

template<auto const& Syscall, typename Arguments>
static vsm::result<byte_io_status> do_byte_io_1(
	native_handle<platform_object_t> const& h,
	Arguments const& a,
	byte_io_state& io_state)
{
	static constexpr bool is_random_access = requires { a.offset; };

	if (a.deadline != deadline::never() &&
		!h.flags[platform_object_t::impl_type::flags::overlapped])
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	vsm_try(event, thread_event::get_for(h));

	step_deadline absolute_deadline = a.deadline;

	HANDLE const handle = unwrap_handle(h.platform_handle);

	LARGE_INTEGER offset_integer;
	LARGE_INTEGER* p_offset_integer = nullptr;

	if constexpr (is_random_access)
	{
		vsm_try_assign(offset_integer.QuadPart, vsm::try_truncate<LONGLONG>(
			a.offset,
			allio_error(error::file_offset_out_of_range)));

		p_offset_integer = &offset_integer;
	}

	io_buffer_layout const layout = a.buffers.get_layout();
	for (io_buffer const io_buffer : read_io_buffers(a.buffers.get_buffers()))
	{
		auto buffer = get_io_buffer_span<std::byte const>(io_buffer, layout);

		if constexpr (is_random_access)
		{
			if (static_cast<fs_size>(offset_integer.QuadPart) > max_file_extent - buffer.size())
			{
				return vsm::unexpected(allio_error(error::file_offset_out_of_range));
			}
		}

		while (!buffer.empty())
		{
			// The maximum transfer size is limited by the range of size_t transferred.
			size_t const max_transfer_size_dynamic = std::min(
				buffer.size(),
				std::numeric_limits<size_t>::max() - io_state.transferred());

			// The maximum transfer size is limited by the range of the ULONG Length parameter.
			ULONG const max_transfer_size = static_cast<ULONG>(std::min(
				max_transfer_size_dynamic,
				static_cast<size_t>(std::numeric_limits<ULONG>::max())));

			vsm_try(relative_deadline, absolute_deadline.step());

			thread_event::io_status_block_t io_status_block;
			NTSTATUS status = Syscall(
				handle,
				event,
				/* ApcRoutine: */ nullptr,
				/* ApcContext: */ nullptr,
				&io_status_block,
				// NtWriteFile takes void* which requires casting away the const.
				const_cast<std::byte*>(buffer.data()),
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
				if constexpr (decltype(is_read_buffer(a.buffers))::value)
				{
					switch (status)
					{
					case STATUS_END_OF_FILE:
					case STATUS_PIPE_BROKEN:
						// For uniformity between Windows and POSIX, broken pipe on read is
						// transformed into end_of_stream.
						return byte_io_status::end_of_stream;
					}
				}

				return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
			}

			size_t const transfer_size = io_status_block.Information;

			if (transfer_size == 0)
			{
				return byte_io_status::end_of_stream;
			}

			io_state.transfer_completed(transfer_size);
			buffer = buffer.subspan(transfer_size);

			if (transfer_size != max_transfer_size)
			{
				// If greedy I/O was requested, the loop is continued until the full transfer is
				// completed, or an error is encountered.
				if (vsm::no_flags(a.flags, io_flags::greedy_byte_io))
				{
					return byte_io_status::success;
				}
			}

			if constexpr (is_random_access)
			{
				offset_integer.QuadPart += static_cast<LONGLONG>(transfer_size);
			}
		}
	}

	return byte_io_status::success;
}

template<typename Result, auto const& Syscall>
static vsm::result<Result> do_byte_io(native_handle<platform_object_t> const& h, auto const& a)
{
	byte_io_state io_state = {};
	return io_state.finalize<Result>(
		a,
		do_byte_io_1<Syscall>(h, a, io_state));
}

} // namespace

vsm::result<size_t> win32::random_read(
	native_handle<platform_object_t> const& h,
	byte_io::random_parameters_t<std::byte> const& a)
{
	return do_byte_io<size_t, win32::NtReadFile>(h, a);
}

vsm::result<size_t> win32::random_write(
	native_handle<platform_object_t> const& h,
	byte_io::random_parameters_t<std::byte const> const& a)
{
	return do_byte_io<size_t, win32::NtWriteFile>(h, a);
}

vsm::result<read_result> win32::stream_read(
	native_handle<platform_object_t> const& h,
	byte_io::stream_parameters_t<std::byte> const& a)
{
	return do_byte_io<read_result, win32::NtReadFile>(h, a);
}

vsm::result<size_t> win32::stream_write(
	native_handle<platform_object_t> const& h,
	byte_io::stream_parameters_t<std::byte const> const& a)
{
	return do_byte_io<size_t, win32::NtWriteFile>(h, a);
}
