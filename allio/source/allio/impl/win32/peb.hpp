#pragma once

#include <vsm/linear.hpp>

namespace allio::win32 {

class unique_peb_lock
{
	vsm::linear<bool, false> m_owns_lock;

public:
	explicit unique_peb_lock(bool const lock = false)
	{
		if (lock)
		{
			this->lock();
		}
	}

	unique_peb_lock(unique_peb_lock&&) = default;

	unique_peb_lock& operator=(unique_peb_lock&& other) & noexcept
	{
		if (m_owns_lock.value)
		{
			unlock();
		}
		m_owns_lock = static_cast<unique_peb_lock&&>(other).m_owns_lock;
		return *this;
	}

	~unique_peb_lock()
	{
		if (m_owns_lock.value)
		{
			unlock();
		}
	}


	[[nodiscard]] bool owns_lock() const
	{
		return m_owns_lock.value;
	}


	void lock();
	void unlock();
};

} // namespace allio::win32
