#include <allio/test/signal.hpp>

#include <atomic>

#include <win32.h>

using namespace allio;

static thread_local DWORD tls_last_exception_code;

static LONG handler(EXCEPTION_POINTERS* const exception_pointers)
{
	std::atomic_signal_fence(std::memory_order_acquire);
	
	tls_last_exception_code = exception_pointers->ExceptionRecord->ExceptionCode;
	
	std::atomic_signal_fence(std::memory_order_release);
	
	return EXCEPTION_EXECUTE_HANDLER;
}

bool allio::capture_signal(signal const signal, vsm::function_view<void()> const callback)
{
	// No others have been needed so far.
	vsm_assert(signal == signal::access_violation);
	
	DWORD const exception_code = EXCEPTION_ACCESS_VIOLATION;
	
	tls_last_exception_code = 0;
	std::atomic_signal_fence(std::memory_order_release);

	// Scope of the exception filter override.
	{
		auto const old_filter = SetUnhandledExceptionFilter(handler);

		vsm_defer
		{
			(void)SetUnhandledExceptionFilter(old_filter);
		};

		callback();
	}

	std::atomic_signal_fence(std::memory_order_acquire);
	DWORD const last_exception_code = tls_last_exception_code;
	
	return last_exception_code == exception_code;
}
