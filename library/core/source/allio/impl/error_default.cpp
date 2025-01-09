#include <allio/error.hpp>

#include <cstdlib>

void allio::unrecoverable_error(std::error_code)
{
	std::abort();
}
