#pragma once

#include <allio/detail/handles/platform_object.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/handles/platform_object.hpp>

namespace allio::win32 {

class thread_event
{
	HANDLE m_event = NULL;
	bool m_reset = false;

public:
	struct io_status_block_t : IO_STATUS_BLOCK
	{
		io_status_block_t()
		{
			Status = STATUS_PENDING;
		}
	};

	struct overlapped_t : OVERLAPPED
	{
		overlapped_t()
		{
			Internal = static_cast<ULONG_PTR>(STATUS_PENDING);
		}
	};


	thread_event() = default;

	explicit thread_event(HANDLE const event)
		: m_event(event)
	{
	}

	thread_event(thread_event&& other) noexcept
		: m_event(other.m_event)
	{
		other.m_event = NULL;
	}

	thread_event& operator=(thread_event&& other) & noexcept
	{
		thread_event local = vsm_move(other);
		std::swap(m_event, local.m_event);
		std::swap(m_reset, local.m_reset);
		return *this;
	}

	~thread_event()
	{
		reset_event();
	}


	[[nodiscard]] static vsm::result<thread_event> get();

	[[nodiscard]] static vsm::result<thread_event> get_for(
		detail::native_handle<detail::platform_object_t> const& h)
	{
		if (h.flags[detail::platform_object_t::impl_type::flags::overlapped])
		{
			return get();
		}

		return {};
	}

	[[nodiscard]] NTSTATUS wait(deadline const deadline);

	[[nodiscard]] NTSTATUS wait_for_io(
		HANDLE const handle,
		io_status_block_t& io_status_block,
		deadline deadline);

	[[nodiscard]] NTSTATUS wait_for_io(
		HANDLE const handle,
		overlapped_t& overlapped,
		deadline deadline);

	template<std::same_as<HANDLE> Handle>
	[[nodiscard]] operator Handle() &
	{
		reset_event();
		m_reset = true;

		// Set the low bit to tell Win32 API functions not to pass ApcContext to NT kernel APIs.
		// This prevents completions from being queued on an IOCP if the handle is attached.
		return m_event == NULL
			? NULL
			: reinterpret_cast<HANDLE>(reinterpret_cast<uintptr_t>(m_event) | 1);
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_event != NULL;
	}

private:
	void reset_event();
};

} // namespace allio::win32
