#include <allio/impl/win32/peb.hpp>

#include <allio/impl/win32/kernel.hpp>

#include <vsm/assert.h>

using namespace allio;
using namespace allio::win32;

void unique_peb_lock::lock()
{
	vsm_assert(!m_owns_lock.value);
	EnterCriticalSection(NtCurrentPeb()->FastPebLock);
	m_owns_lock.value = true;
}

void unique_peb_lock::unlock()
{
	vsm_assert(m_owns_lock.value);
	LeaveCriticalSection(NtCurrentPeb()->FastPebLock);
	m_owns_lock.value = false;
}
