#include <win32.h>

namespace allio::test {

bool catch_access_violation(void(*function)(void*), void* context);

bool catch_access_violation(void(* const function)(void*), void* const context)
{
	bool r = true;

	__try
	{
		function(context);
	}
	__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		r = false;
	}

	return r;
}

} // namespace allio::test
