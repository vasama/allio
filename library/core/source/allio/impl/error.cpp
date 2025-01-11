#include <allio/error.hpp>

#include <format>

#include <cinttypes>

using namespace allio;

char const* detail::error_category::name() const noexcept
{
	return error_category_name;
}

std::string detail::error_category::message(int const code) const
{
	switch (static_cast<error>(code))
	{
	// Generic
	case error::none:
		return "The operation completed successfully.";
	case error::unknown_failure:
		return "An unexpected failure occurred.";
	case error::unsupported_operation:
		return "The requested operation is not supported.";
	case error::unsupported_input_format:
		return "The provided data format is not supported.";
	case error::device_or_resource_busy:
		return "The device or resource is busy.";

	// Arguments
	case error::invalid_argument:
		return "The specified argument was invalid.";
	case error::argument_too_long:
		return "The specified argument was too long.";

	// Storage
	case error::not_enough_memory:
		return "The required heap memory could not be allocated.";
	case error::no_buffer_space:
		return "The size of the provided buffer was insufficient.";

	// Encoding
	case error::unsupported_encoding:
		return "The requested encoding is not supported.";
	case error::invalid_encoding:
		return "The specified string is improperly encoded.";

	// Handle
	case error::handle_is_null:
		return "The provided handle is null";
	case error::handle_is_not_null:
		return "The provided handle is not null.";
	case error::handle_is_not_multiplexable:
		return "The provided handle is not multiplexable.";

	// Operations
	case error::operation_pending:
		return "The operation was started successfully.";
	case error::operation_canceled:
		return "The operation was canceled.";
	case error::operation_timed_out:
		return "The operation timed out and was canceled.";

	// Filesystem
	case error::filename_too_long:
		return "The specified path was too long.";
	case error::invalid_path:
		return "The specified path was invalid.";
	case error::invalid_current_directory:
		return "The current working directory is not valid.";
	case error::unrepresentable_path:
		return "The path is not representable in the requested format.";
	case error::file_offset_out_of_range:
		return "The specified file offset is beyond the maximum extent of the file.";

	// Memory
	case error::invalid_address:
		return "The specified memory address is not valid.";
	case error::unsupported_page_level:
		return "The specified page level is not supported";
	case error::virtual_address_not_available:
		return "The requested virtual address range is not available.";
	case error::not_enough_address_space:
		return "The required virtual address space could not be allocated.";

	// Process
	case error::process_is_current_process:
		return "The specified handle refers to the current process.";
	case error::process_arguments_too_long:
		return "The specified process arguments are too long.";
	case error::process_exit_code_not_available:
		return "The operation was completed successfully but the process exit code is not available.";

	// Sockets
	case error::socket_already_bound:
		return "The specified socket was already bound to an address.";
	}
	return std::format("Unrecognized error code: {:08X}", static_cast<uint32_t>(code));
}

std::error_condition detail::error_category::default_error_condition(int const code) const noexcept
{
	switch (static_cast<error>(code))
	{
	// Generic
	case error::none:
		return std::error_condition(std::errc{});
	case error::unknown_failure:
		break;
	case error::unsupported_operation:
		return std::error_condition(std::errc::operation_not_supported);
	case error::unsupported_input_format:
		return std::error_condition(std::errc::invalid_argument);
	case error::device_or_resource_busy:
		return std::error_condition(std::errc::device_or_resource_busy);

	// Arguments
	case error::invalid_argument:
	case error::argument_too_long:
		return std::error_condition(std::errc::invalid_argument);

	// Storage
	case error::not_enough_memory:
		return std::error_condition(std::errc::not_enough_memory);
	case error::no_buffer_space:
		return std::error_condition(std::errc::no_buffer_space);

	// Encoding
	case error::unsupported_encoding:
		break;
	case error::invalid_encoding:
		break;

	// Handle
	case error::handle_is_null:
		break;
	case error::handle_is_not_null:
		break;
	case error::handle_is_not_multiplexable:
		break;

	// Operations
	case error::operation_pending:
		break;
	case error::operation_canceled:
		return std::error_condition(std::errc::operation_canceled);
	case error::operation_timed_out:
		return std::error_condition(std::errc::timed_out);

	// Filesystem
	case error::filename_too_long:
		return std::error_condition(std::errc::filename_too_long);
	case error::invalid_path:
		break;
	case error::invalid_current_directory:
		break;
	case error::unrepresentable_path:
		break;
	case error::file_offset_out_of_range:
		break;

	// Memory
	case error::invalid_address:
		return std::error_condition(std::errc::bad_address);
	case error::unsupported_page_level:
		break;
	case error::virtual_address_not_available:
		break;
	case error::not_enough_address_space:
		break;

	// Process
	case error::process_is_current_process:
		break;
	case error::process_arguments_too_long:
		return std::error_condition(std::errc::argument_list_too_long);
	case error::process_exit_code_not_available:
		break;

	// Sockets
	case error::socket_already_bound:
		break;
	}
	return std::error_condition(code, *this);
}

detail::error_category const detail::error_category_instance;


namespace {

struct default_error_handler final : error_handler
{
	void handle_error(error_information const& information) override
	{
		//TODO: Handle error somehow.
		(void)information;
	}
};

static constinit default_error_handler default_error_handler_instance;
static constinit error_handler* g_error_handler = &default_error_handler_instance;

} // namespace

void allio::set_error_handler(error_handler* const handler) noexcept
{
	g_error_handler = handler != nullptr
		? handler
		: &default_error_handler_instance;
}

error_handler& allio::get_error_handler() noexcept
{
	return *g_error_handler;
}
