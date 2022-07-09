#include <allio/error.hpp>

using namespace allio;

extern "C" __attribute__((weak))
void allio_unrecoverable_error(std::error_code const error)
{
	detail::unrecoverable_error_default(error);
}

void detail::unrecoverable_error(std::error_code const error)
{
	allio_unrecoverable_error(error);
}
