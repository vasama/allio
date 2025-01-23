#include <allio/detail/exceptions.hpp>

#include <allio/error.hpp>

#include <cstring>

using namespace allio;
using namespace allio::detail;

void detail::throw_error(std::error_code const e)
{
	if (is_error(e, error::not_enough_memory))
	{
		// Throw bad_alloc for exactly allio::error::not_enough_memory which is only produced as a
		// result of non-throwing operator new failure.
		throw std::bad_alloc();
	}
	else
	{
		throw std::system_error(e);
	}
}
