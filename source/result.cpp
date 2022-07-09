#include <allio/result.hpp>

//#include <format>

using namespace allio;

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
	}

	return "Unrecognized error code.";
	//return std::format("Unrecognized error code ({:#08x}).", static_cast<uint32_t>(code));
}

detail::error_category const detail::error_category_instance;
