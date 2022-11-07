#pragma once

#include <allio/deadline.hpp>

namespace allio {

template<typename Timespec>
Timespec make_timespec(deadline const deadline)
{
	allio_ASSERT(deadline.is_relative());
	auto const duration = deadline.relative();

	Timespec timespec;
	timespec.tv_sec = std::chrono::duration_cast<std::chrono::duration<uint64_t>>(duration).count();
	timespec.tv_nsec = std::chrono::duration_cast<std::chrono::duration<uint64_t, std::nano>>(duration).count();
	return timespec;
}

template<typename Timespec>
class kernel_timeout
{
	Timespec m_timespec;
	Timespec const* m_p_timespec;

public:
	kernel_timeout()
		: m_p_timespec(nullptr)
	{
	}

	kernel_timeout(deadline const deadline)
	{
		set(deadline);
	}

	Timespec* get()
	{
		return m_p_timespec;
	}

	Timespec const* get() const
	{
		return m_p_timespec;
	}

	void set(deadline const deadline)
	{
		allio_ASSERT(deadline.is_relative());
		if (deadline == allio::deadline::never())
		{
			m_p_timespec = nullptr;
		}
		else
		{
			if (deadline == allio::deadline::instant())
			{
				m_timespec = {};
			}
			else
			{
				m_timespec = make_timespec<Timespec>();
			}
			m_p_timespec = &m_timespec;
		}
	}

	void set_instant()
	{
		m_timespec.tv_sec = 0;
		m_timespec.tv_nsec = 0;
		m_p_timespec = &m_timespec;
	}

	void set_never()
	{
		m_p_timespec = nullptr;
	}

	operator Timespec*()
	{
		return m_p_timespec;
	}

	operator Timespec const*() const
	{
		return m_p_timespec;
	}
}

} // namespace allio
