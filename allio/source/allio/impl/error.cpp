#include <allio/error.hpp>

#include <cinttypes>

using namespace allio;

void detail::unrecoverable_error_default(std::error_code const error, std::source_location const location)
{
	std::abort();
}

char const* detail::error_category::name() const noexcept
{
	return "allio";
}

std::string detail::error_category::message(int const code) const
{
	switch (static_cast<error>(code))
	{
	case error::async_operation_cancelled:
		return "Asynchronous operation was cancelled.";

	case error::unsupported_multiplexer_handle_relation:
		return "The combination of multiplexer and handle is not supported by the relation provider.";

	case error::too_many_concurrent_async_operations:
		return "Too many asynchronous operations at once.";

	case error::handle_is_null:
		return "The provided handle is null";

	case error::handle_is_not_null:
		return "The provided handle is not null.";

	case error::handle_is_not_multiplexable:
		return "The provided handle is not multiplexable.";

	case error::process_is_current_process:
		return "The provided process handle refers to the current process.";
	}

	char string[64];
	int const string_size = sprintf(string,
		"Unrecognized error code: %08" PRIX32, static_cast<uint32_t>(code));

	return std::string(string, string_size);
}

std::error_condition detail::error_category::default_error_condition(int const code) const noexcept
{
	switch (static_cast<error>(code))
	{
	case error::async_operation_timed_out:
		return std::error_condition(std::errc::timed_out);
	}
	
	return std::error_condition(code , *this);
}

detail::error_category const detail::error_category_instance;
