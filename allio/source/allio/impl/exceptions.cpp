#include <allio/detail/exceptions.hpp>

#include <allio/error.hpp>

using namespace allio;
using namespace allio::detail;

static bool is_allio_category(std::error_category const& category)
{
	return
		&category == &error_category_instance ||
		strcmp(category.name(), error_category_name) == 0;
}

static bool is_not_enough_memory(std::error const e)
{
	return
		is_allio_category(e.category()) &&
		static_cast<error>(e.value()) == error::not_enough_memory;
}

void detail::throw_error(std::error_code const e)
{
	if (is_not_enough_memory(e))
	{
		// Throw bad_alloc for exactly allio::error::not_enough_memory
		// which is only produced as a result of non-throwing operator new failure.
		throw std::bad_alloc();
	}
	else
	{
		throw std::system_error(error);
	}
}
