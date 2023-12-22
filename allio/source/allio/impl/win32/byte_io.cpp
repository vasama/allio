#include <allio/impl/win32/byte_io.hpp>

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/step_deadline.hpp>
#include <allio/win32/handles/platform_object.hpp>

#include <vsm/out_resource.hpp>

#include <thread>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

namespace {

class thread_event
{
	HANDLE m_event = NULL;
	bool m_reset = false;

public:
	thread_event() = default;

	explicit thread_event(HANDLE const event)
		: m_event(event)
	{
	}

	thread_event(thread_event&& other)
		: m_event(other.m_event)
	{
		other.m_event = NULL;
	}

	thread_event& operator=(thread_event&& other) &
	{
		reset_event();

		m_event = other.m_event;
		other.m_event = NULL;

		return *this;
	}

	~thread_event()
	{
		reset_event();
	}


	[[nodiscard]] HANDLE get()
	{
		m_reset = true;
		return m_event;
	}

	[[nodiscard]] NTSTATUS wait(deadline const deadline)
	{
		NTSTATUS const status = win32::NtWaitForSingleObject(
			m_event,
			/* Alertable: */ false,
			kernel_timeout(deadline));

		if (NT_SUCCESS(status))
		{
			m_reset = false;
		}

		return status;
	}

	[[nodiscard]] NTSTATUS wait_for_io(
		HANDLE const handle,
		IO_STATUS_BLOCK& io_status_block,
		deadline deadline)
	{
		if (io_status_block.Status == STATUS_PENDING)
		{
			NTSTATUS status = wait(deadline);

			if (status != STATUS_WAIT_0)
			{
				if (io_status_block.Status == STATUS_PENDING)
				{
					status = NtCancelIoFileEx(
						handle,
						&io_status_block,
						&io_status_block);
	
					if (NT_SUCCESS(status))
					{
						vsm_assert(io_status_block.Status != STATUS_PENDING);
					}
				}
			}

			while (io_status_block.Status == STATUS_PENDING)
			{
				std::this_thread::yield();
			}
		}
		return io_status_block.Status;
	}

private:
	void reset_event()
	{
		if (m_event != NULL && m_reset)
		{
			NTSTATUS const status = NtResetEvent(
				m_event,
				/* PreviousState: */ nullptr);

			vsm_assert(NT_SUCCESS(status));
		}
	}
};

} // namespace

//TODO: Apply event based overlapped I/O to other synchronous I/O operations.
static vsm::result<thread_event> get_thread_event()
{
	auto const make_event = []() -> vsm::result<unique_handle>
	{
		vsm::result<unique_handle> r(vsm::result_value);

		NTSTATUS const status = NtCreateEvent(
			vsm::out_resource(*r),
			EVENT_ALL_ACCESS,
			/* ObjectAttributes: */ nullptr,
			SynchronizationEvent,
			/* InitialState: */ false);

		if (!NT_SUCCESS(status))
		{
			r = vsm::unexpected(static_cast<kernel_error>(status));
		}

		return r;
	};

	static auto const event = make_event();
	return event.transform([](unique_handle const& h)
	{
		return thread_event(h.get());
	});
}

template<auto const& Syscall>
static vsm::result<size_t> do_byte_io(
	native_handle<platform_object_t> const& h,
	auto const& a)
{
	static constexpr bool is_random = requires { a.offset; };

	thread_event event;
	if (h.flags[platform_object_t::impl_type::flags::synchronous])
	{
		vsm_try_assign(event, get_thread_event());
	}

	step_deadline absolute_deadline = a.deadline;

	HANDLE const handle = unwrap_handle(h.platform_handle);

	LARGE_INTEGER offset_integer;
	LARGE_INTEGER* p_offset_integer = nullptr;

	if constexpr (is_random)
	{
		offset_integer.QuadPart = a.offset;
		p_offset_integer = &offset_integer;
	}

	auto const transfer_some = [&](
		auto const data,
		ULONG const max_transfer_size) -> vsm::result<size_t>
	{
		vsm_try(relative_deadline, absolute_deadline.step());

		IO_STATUS_BLOCK io_status_block;
		NTSTATUS status = Syscall(
			handle,
			event.get(),
			/* ApcRoutine: */ nullptr,
			/* ApcContext: */ nullptr,
			&io_status_block,
			// NtWriteFile takes void* which requires casting away the const.
			const_cast<std::byte*>(data),
			max_transfer_size,
			p_offset_integer,
			/* Key: */ 0);

		if (status == STATUS_PENDING)
		{
			status = event.wait_for_io(
				handle,
				io_status_block,
				relative_deadline);
		}

		if (!NT_SUCCESS(status))
		{
			return vsm::unexpected(static_cast<kernel_error>(status));
		}

		return io_status_block.Information;
	};

	size_t transferred = 0;
	for (auto buffer : a.buffers.buffers())
	{
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
