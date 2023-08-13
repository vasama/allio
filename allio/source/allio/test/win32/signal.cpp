#include <allio/test/signal.hpp>

#include <vsm/assert.h>
#include <vsm/defer.hpp>

#include <win32.h>

using namespace allio;

bool allio::capture_signal(signal const signal, vsm::function_view<void()> const callback)
{
	// No others have been needed so far.
	vsm_assert(signal == signal::access_violation);

	bool got_exception = false;

	__try
	{
		callback();
	}
	__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		got_exception = true;
	}

	return got_exception;
}
