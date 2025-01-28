#include <vsm/platform.h>

#include <Windows.h>

vsm_clang_diagnostic(ignored "-Wlanguage-extension-token")

namespace allio::test {

bool catch_access_violation(void(*function)(void*), void* context);

bool catch_access_violation(void(* const function)(void*), void* const context)
{
	bool r = true;

	#define allio_filter_exception(...) ( \
		(__VA_ARGS__) \
			? EXCEPTION_EXECUTE_HANDLER \
			: EXCEPTION_CONTINUE_SEARCH \
	)

	__try
	{
		function(context);
	}
	__except (allio_filter_exception(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION))
	{
		r = false;
	}

	return r;
}

} // namespace allio::test
