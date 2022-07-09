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
		reset_event();

		m_event = other.m_event;
		other.m_event = NULL;

		return *this;
	}

	~thread_event()
	{
		reset_event();
	}


	[[nodiscard]] static vsm::result<thread_event> get();

	[[nodiscard]] static vsm::result<thread_event> get_for(detail::native_handle<detail::platform_object_t> const& h)
	{
		if (!h.flags[detail::platform_object_t::impl_type::flags::synchronous])
		{
			return get();
		}
	
		return {};
	}

	[[nodiscard]] NTSTATUS wait(deadline const deadline);
	[[nodiscard]] NTSTATUS wait_for_io(HANDLE const handle, IO_STATUS_BLOCK& io_status_block, deadline deadline);
	[[nodiscard]] NTSTATUS wait_for_io(HANDLE const handle, OVERLAPPED& overlapped, deadline deadline);

	template<std::same_as<HANDLE> Handle>
	[[nodiscard]] operator Handle() &
	{
		m_reset = true;
		return m_event;
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_event != NULL;
	}

private:
	void reset_event();
};

} // namespace allio::win32
