#include <allio/error.hpp>

extern "C" __attribute__((weak))
void allio_unrecoverable_error(std::error_code const error, std::source_location const location)
{
	detail::unrecoverable_error_default(error, location);
}

void detail::unrecoverable_error(std::error_code const error, std::source_location const location)
{
	allio_unrecoverable_error(error, location);
}
