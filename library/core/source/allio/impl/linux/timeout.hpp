#pragma once

#include <allio/linux/timespec.hpp>

namespace allio {

template<typename Timespec>
class kernel_timeout
{
	struct instant_t
	{
		explicit instant_t() = default;
	};

	Timespec m_timespec;
	Timespec* m_p_timespec;

public:
	kernel_timeout()
		: m_p_timespec(nullptr)
	{
	}

	kernel_timeout(deadline const deadline)
	{
		set(deadline);
	}

	kernel_timeout(instant_t)
	{
		set_instant();
	}

	kernel_timeout(kernel_timeout const&) = delete;
	kernel_timeout& operator=(kernel_timeout const&) = delete;

	[[nodiscard]] static kernel_timeout instant()
	{
		return kernel_timeout(instant_t());
	}

	[[nodiscard]] Timespec* get()
	{
		return m_p_timespec;
	}

	[[nodiscard]] Timespec const* get() const
	{
		return m_p_timespec;
	}

	void set(deadline const deadline)
	{
		vsm_assert(deadline.is_relative());
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
				m_timespec = make_timespec<Timespec>(deadline);
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

	[[nodiscard]] operator Timespec*()
	{
		return m_p_timespec;
	}

	[[nodiscard]] operator Timespec const*() const
	{
		return m_p_timespec;
	}

private:
};

} // namespace allio
