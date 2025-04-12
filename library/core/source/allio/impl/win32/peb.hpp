#pragma once

#include <vsm/utility.hpp>

namespace allio::win32 {

class unique_peb_lock
{
	bool m_owns_lock = false;

public:
	explicit unique_peb_lock(bool const lock = false)
	{
		if (lock)
		{
			this->lock();
		}
	}

	unique_peb_lock(unique_peb_lock&& other)
		: m_owns_lock(other.m_owns_lock)
	{
		other.m_owns_lock = false;
	}

	unique_peb_lock& operator=(unique_peb_lock&& other) &
	{
		unique_peb_lock local = vsm_move(other);
		std::swap(m_owns_lock, local.m_owns_lock);
		return *this;
	}

	~unique_peb_lock()
	{
		if (m_owns_lock)
		{
			unlock();
		}
	}


	[[nodiscard]] bool owns_lock() const
	{
		return m_owns_lock;
	}


	void lock();
	void unlock();
};

} // namespace allio::win32
