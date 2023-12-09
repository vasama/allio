#pragma once

#include <vsm/assert.h>
#include <vsm/atomic.hpp>

#include <thread>

namespace allio::detail {

class externally_synchronized
{
	vsm::atomic<std::thread::id> m_thread_id = std::thread::id();

public:
	externally_synchronized() = default;

	externally_synchronized(externally_synchronized const&)
	{
	}
	
	externally_synchronized& operator=(externally_synchronized const&) &
	{
		return *this;
	}


	void external_synchronization_acquired() &
	{
		vsm_assert(m_thread_id.load(std::memory_order_relaxed) == std::thread::id());
		m_thread_id.store(std::this_thread::get_id(), std::memory_order_release);
	}

	void external_synchronization_released() &
	{
		m_thread_id.store(std::thread::id(), std::memory_order_release);
	}

	[[nodiscard]] bool is_externally_synchronized() const
	{
		return m_thread_id.load(std::memory_order_acquire) == std::this_thread::get_id();
	}
};

template<typename ExternallySynchronized>
class scoped_synchronization
{
	ExternallySynchronized& m_object;

public:
	scoped_synchronization(ExternallySynchronized& object)
		: m_object(object)
	{
		m_object.external_synchronization_acquired();
	}

	scoped_synchronization(scoped_synchronization const&) = delete;
	scoped_synchronization& operator=(scoped_synchronization const&) = delete;

	~scoped_synchronization()
	{
		m_object.external_synchronization_released();
	}
};

} // namespace allio::detail
