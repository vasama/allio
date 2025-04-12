#include <allio/error.hpp>
#include <allio/impl/error_encoding_impl.hpp>

#include <format>

#include <cinttypes>

using namespace allio;
using namespace allio::detail;

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
	case error::invariant_violation:
		return "An internal invariant was violated.";
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
	case error::maximum_capacity_exceeded:
		return "The maximum capacity of the container was exceeded.";
	case error::insufficient_alignment:
		return "The minimum alignment requirements were not met.";

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
	case error::handle_cannot_be_detached:
		return "The provided handle cannot be detached.";

	// Operations
	case error::operation_pending:
		return "The operation was started successfully.";
	case error::operation_canceled:
		return "The operation was canceled.";
	case error::operation_timed_out:
		return "The operation timed out and was canceled.";

	// Byte I/O
	case error::end_of_stream:
		return "The end of the byte stream was reached.";
	case error::too_many_io_buffers:
		return "Too many byte I/O buffers were specified for the operation.";
	case error::io_size_out_of_range:
		return "The size of the requested operation is too large.";

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
		return std::errc{};
	case error::unknown_failure:
		break;
	case error::invariant_violation:
		break;
	case error::unsupported_operation:
		return std::errc::operation_not_supported;
	case error::unsupported_input_format:
		return std::errc::invalid_argument;
	case error::device_or_resource_busy:
		return std::errc::device_or_resource_busy;

	// Arguments
	case error::invalid_argument:
	case error::argument_too_long:
		return std::errc::invalid_argument;

	// Storage
	case error::not_enough_memory:
		return std::errc::not_enough_memory;
	case error::no_buffer_space:
		return std::errc::no_buffer_space;
	case error::maximum_capacity_exceeded:
		break;
	case error::insufficient_alignment:
		break;

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
	case error::handle_cannot_be_detached:
		break;

	// Operations
	case error::operation_pending:
		break;
	case error::operation_canceled:
		return std::errc::operation_canceled;
	case error::operation_timed_out:
		return std::errc::timed_out;

	// Byte I/O
	case error::end_of_stream:
		break;
	case error::too_many_io_buffers:
		break;
	case error::io_size_out_of_range:
		break;

	// Filesystem
	case error::filename_too_long:
		return std::errc::filename_too_long;
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
		return std::errc::bad_address;
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
		return std::errc::argument_list_too_long;

	// Sockets
	case error::socket_already_bound:
		break;
	}
	return std::error_condition(code, *this);
}

std::error_code detail::error_category::unwrap(std::error_code const ec) const noexcept
{
	vsm_assert(ec.category() == *this); //PRECONDITION
	return ec;
}

detail::error_category const detail::error_category_instance;


template<>
uint32_t ec::encode_error_code(error const e)
{
	//TODO: Static assert that e is never out of range.
	return static_cast<uint32_t>(e);
}

template<>
error ec::decode_error_code(uint32_t const e)
{
	return static_cast<error>(e);
}

template class ec::encoded_error_category<allio_error_encoding, allio::error>;


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


static bool is_allio_error_category(std::error_category const& category)
{
	return
		category == error_category_instance ||
		std::strcmp(category.name(), error_category_name) == 0;
}

bool detail::is_error_code(std::error_code ec, error const e)
{
	if (is_allio_error_category(ec.category()))
	{
		ec = static_cast<error_category_base const&>(ec.category()).unwrap(ec);
		return e == static_cast<error>(ec.value());
	}

	return false;
}

bool detail::is_error_code(std::error_code const ec, std::errc const e)
{
	return e == ec.default_error_condition();
}
