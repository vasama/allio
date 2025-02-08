#include <allio/win32/detail/iocp/byte_io.hpp>

#include <allio/detail/byte_io_buffer_range.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/win32/kernel.hpp>

#include <vsm/numeric.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

namespace {

static constexpr fs_size max_file_extent =
	static_cast<fs_size>(std::numeric_limits<LONGLONG>::max());

template<auto const& Syscall>
class iocp_byte_io
{
	iocp_byte_io_state& m_state;

	new_io_buffer_layout m_buffers_layout;
	new_io_buffer_range m_buffers;

public:
	explicit iocp_byte_io(iocp_byte_io_state& state)
		: m_state(state)
	{
	}

	template<typename Arguments>
	io_result<size_t> submit(
		native_handle<platform_object_t> const& h,
		Arguments const& a,
		io_handler<iocp_multiplexer>& handler)
	{
		if (get_file_offset(a) >= max_file_extent)
		{
			return vsm::unexpected(allio_error(error::file_offset_out_of_range));
		}

		m_buffers_layout = a.buffers.get_layout();
		m_buffers = read_io_buffers(a.buffers.get_buffers());

		if (m_buffers.size() == 0)
		{
			//TODO: Add a unit test for this to make sure the behaviour matches on Linux.
			return 0;
		}

		return transform_result(a, [&]() -> io_result<void>
		{
			m_state.absolute_deadline = a.deadline;

			m_state.transferred = 0;
			m_state.buffer_index = 0;
			m_state.buffer_visit = 0;

			m_state.io_status_block.bind(handler);

			return submit_loop(h, a);
		}());
	}

	template<typename Arguments>
	io_result<size_t> notify(
		native_handle<platform_object_t> const& h,
		Arguments const& a,
		iocp_multiplexer::io_status_type const status)
	{
		vsm_assert(&status.slot == &m_state.io_status_block);

		return transform_result(a, [&]() -> io_result<void>
		{
			size_t const transfer_size = m_state.io_status_block->Information;

			if (transfer_size == 0)
			{
				return {};
			}

			m_buffers_layout = a.buffers.get_layout();
			m_buffers = read_io_buffers(a.buffers.get_buffers());

			std::span<std::byte const> const buffer = get_current_buffer();
			size_t const max_transfer_size = get_max_transfer_size(buffer.size());
			vsm_try_void(handle_completion(a, transfer_size, max_transfer_size));

			return submit_loop(h, a);
		}());
	}

private:
	template<typename Arguments>
	constexpr fs_size get_file_offset(Arguments const& a)
	{
		if constexpr (requires { a.offset; })
		{
			return a.offset;
		}
		else
		{
			return 0;
		}
	}

	template<typename Arguments>
	io_result<void> handle_completion(
		Arguments const& a,
		size_t const transfer_size,
		size_t const max_transfer_size)
	{
		vsm_assert(transfer_size != 0);
		vsm_assert(max_transfer_size != 0);

		vsm_assert(transfer_size <= max_transfer_size);

		// The addition of the transferred size to the total cannot wrap around.
		vsm_assert(m_state.transferred <= static_cast<size_t>(-1) - transfer_size);

		m_state.transferred += transfer_size;
		++m_state.buffer_visit;

		if (transfer_size != max_transfer_size)
		{
			if (vsm::any_flags(a.flags, io_flags::greedy_byte_io))
			{
				return {};
			}
			else
			{
				// This error is handled locally and should never propagate to the user.
				return vsm::unexpected(allio_error(error::invariant_violation));
			}
		}

		if (m_state.transferred == static_cast<size_t>(-1))
		{
			return vsm::unexpected(allio_error(error::io_size_out_of_range));
		}

		if (get_file_offset(a) >= max_file_extent - m_state.transferred)
		{
			return vsm::unexpected(allio_error(error::file_offset_out_of_range));
		}

		// All of these conditions are non-erroneous cases where some number of suboperations have
		// succeeded but no further suboperations may be submitted.
		if (transfer_size != max_transfer_size ||
			m_state.transferred == static_cast<size_t>(-1) ||
			get_file_offset(a) >= max_file_extent - m_state.transferred)
		{
			// This error is handled locally and should never propagate to the user.
			return vsm::unexpected(allio_error(error::invariant_violation));
		}

		return {};
	}

	template<typename Arguments>
	io_result<void> submit_loop(native_handle<platform_object_t> const& h, Arguments const& a)
	{
		vsm_assert(m_state.buffer_index < m_buffers.size());

		HANDLE const handle = unwrap_handle(h.platform_handle);

		do
		{
			std::span<std::byte const> buffer = get_current_buffer();

			while (!buffer.empty())
			{
				vsm_try(relative_deadline, m_state.absolute_deadline.step());

				//TODO: Implement timeouts via timed cancellation.
				(void)relative_deadline;

				ULONG const max_transfer_size = get_max_transfer_size(buffer.size());

				LARGE_INTEGER offset_integer;
				LARGE_INTEGER* p_offset_integer = nullptr;

				if constexpr (requires { a.offset; })
				{
					// The validity of this conversion is ensured by
					// 1. the initial offset check in submit, and
					// 2. the addition check in handle_completion.
					offset_integer.QuadPart = vsm::truncating(a.offset + m_state.transferred);

					p_offset_integer = &offset_integer;
				}

				IO_STATUS_BLOCK& io_status_block = *m_state.io_status_block;

				//TODO: Should STATUS_END_OF_FILE ever be handled?
				NTSTATUS const status = Syscall(
					handle,
					/* Event: */ NULL,
					/* ApcRoutine: */ nullptr,
					/* ApcContext: */ &io_status_block,
					&io_status_block,
					// NtWriteFile takes void* which requires casting away the const.
					const_cast<std::byte*>(buffer.data()),
					max_transfer_size,
					p_offset_integer,
					/* Key: */ nullptr);

				if (status == STATUS_PENDING ||
					!iocp_multiplexer::supports_synchronous_completion(h))
				{
					return vsm::unexpected(io_notify_status::submitted);
				}

				if (!NT_SUCCESS(status))
				{
					return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
				}

				size_t const transfer_size = m_state.io_status_block->Information;

				if (transfer_size == 0)
				{
					return {};
				}

				vsm_try_void(handle_completion(a, transfer_size, max_transfer_size));

				buffer = buffer.subspan(transfer_size);

				static constexpr auto equivalent = [](auto const lhs, auto const rhs)
				{
					return
						(lhs.size() == 0 && rhs.size() == 0) ||
						(lhs.data() == rhs.data() && lhs.size() == rhs.size());
				};
				vsm_assert(equivalent(buffer, get_current_buffer()));
			}
		}
		while (++m_state.buffer_index < m_buffers.size());

		return {};
	}

	std::span<std::byte const> get_current_buffer() const
	{
		static constexpr size_t visit_multiplier = std::numeric_limits<ULONG>::max();

		auto const io_buffer = m_buffers[m_state.buffer_index];
		auto const full_span = get_io_buffer_span<std::byte const>(io_buffer, m_buffers_layout);

		if (full_span.size() / visit_multiplier < m_state.buffer_visit)
		{
			return {};
		}

		return full_span.subspan(m_state.buffer_visit * visit_multiplier);
	}

	ULONG get_max_transfer_size(size_t const buffer_size) const
	{
		// The maximum transfer size is limited by the range of size_t transferred.
		size_t const limit_1 = std::min(
			buffer_size,
			std::numeric_limits<size_t>::max() - m_state.transferred);

		// The maximum transfer size is limited by the range of the ULONG Length parameter.
		ULONG const limit_2 = static_cast<ULONG>(std::min(
			limit_1,
			static_cast<size_t>(std::numeric_limits<ULONG>::max())));

		return limit_2;
	}

	template<typename Arguments>
	io_result<size_t> transform_result(Arguments const& a, io_result<void> const& result)
	{
		if (!result)
		{
			if (result.error().get_io_notify_status() == io_notify_status::submitted ||
				m_state.transferred == 0 || vsm::any_flags(a.flags, io_flags::greedy_byte_io))
			{
				return vsm::unexpected(result.error());
			}
		}

		if (m_state.transferred == 0)
		{
			return vsm::unexpected(allio_error(error::end_of_stream));
		}

		return m_state.transferred;
	}
};

template<template<typename> typename Template>
iocp_byte_io<win32::NtReadFile> _detect_byte_io(Template<std::byte> const&);

template<template<typename> typename Template>
iocp_byte_io<win32::NtWriteFile> _detect_byte_io(Template<std::byte const> const&);

template<typename T>
using detect_byte_io = decltype(_detect_byte_io(std::declval<T const&>()));

} // namespace

template<typename Arguments>
io_result<size_t> iocp_byte_io_state::submit(
	H const& h,
	Arguments const& a,
	io_handler<M>& handler)
{
	return detect_byte_io<Arguments>(*this).submit(h, a, handler);
}

template<typename Arguments>
io_result<size_t> iocp_byte_io_state::notify(
	H const& h,
	Arguments const& a,
	M::io_status_type status)
{
	return detect_byte_io<Arguments>(*this).notify(h, a, status);
}

void iocp_byte_io_state::cancel(M& m, H const& h)
{
	//TODO: There is a race condition here when cancel is called between two NT I/Os.
	(void)m.cancel_io(io_status_block, h.platform_handle);
}


#define allio_byte_io_instance(arguments_type) \
	template io_result<size_t> iocp_byte_io_state::submit<arguments_type>( \
		H const& h, \
		arguments_type const& a, \
		io_handler<M>& handler); \
	template io_result<size_t> iocp_byte_io_state::notify<arguments_type>( \
		H const& h, \
		arguments_type const& a, \
		M::io_status_type status) \

allio_byte_io_instance(byte_io::random_parameters_t<std::byte>);
allio_byte_io_instance(byte_io::random_parameters_t<std::byte const>);
allio_byte_io_instance(byte_io::stream_parameters_t<std::byte>);
allio_byte_io_instance(byte_io::stream_parameters_t<std::byte const>);
