#pragma once

#include <exception>

namespace allio::test {

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
