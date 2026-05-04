#pragma once

#include <exception>

namespace allio::test {

// This is a workaround for the following two GCC bugs:
// * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=101027
// * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=116775
//
// Because of these bugs, and due to implementation details of Catch2 assertion macros, the direct
// use of await expressions inside REQUIRE_THROWS_MATCHES is problematic. Instead, a two step
// solution of first capturing the exception, and later rethrowing it is used.
#define allio_test_catch_exception(variable, ...) \
	std::exception_ptr variable; \
	\
	try \
	{ \
		(void)(__VA_ARGS__); \
	} \
	catch (...) \
	{ \
		variable = std::current_exception(); \
	} \

inline void rethrow_exception(std::exception_ptr const& exception)
{
	if (exception)
	{
		std::rethrow_exception(exception);
	}
}

} // namespace allio::test
