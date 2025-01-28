#include <allio/error.hpp>

#include <iostream>

//TODO: Enable stacktrace support
#if allio_config_std_stacktrace
#	include <stacktrace>
#endif

#include <cstdlib>

void allio::unrecoverable_error(std::error_code const error) noexcept
{
	std::cerr << "unrecoverable error encountered in allio:\n" << error << "\n\n";

#if allio_config_std_stacktrace
	std::cerr << std::stacktrace::current() << std::endl;
#endif

	std::terminate();
}
