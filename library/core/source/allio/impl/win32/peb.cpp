#include <allio/impl/win32/peb.hpp>

#include <allio/impl/win32/kernel.hpp>

#include <vsm/assert.h>

using namespace allio;
using namespace allio::win32;

void unique_peb_lock::lock()
{
	vsm_assert(!m_owns_lock);
	EnterCriticalSection(NtCurrentPeb()->FastPebLock);
	m_owns_lock = true;
}

void unique_peb_lock::unlock()
{
	vsm_assert(m_owns_lock);
	LeaveCriticalSection(NtCurrentPeb()->FastPebLock);
	m_owns_lock = false;
}
