#pragma once

#include <allio/detail/api.hpp>

#include <vsm/concepts.hpp>
#include <vsm/result.hpp>

#include <cstring>

namespace allio {

void unrecoverable_error(std::error_code error) noexcept;

namespace detail {

inline void unrecoverable(vsm::result<void> const& e) noexcept
{
	if (!e)
	{
		unrecoverable_error(e.error());
	}
}

decltype(auto) unrecoverable(auto&& r, auto&& default_value) noexcept
{
	if (r)
	{
		return *vsm_forward(r);
	}

	unrecoverable_error(r.error());
	return vsm_forward(default_value);
}


inline constexpr char error_category_name[] = "allio";

struct error_category final : std::error_category
{
	char const* name() const noexcept override;
	std::string message(int const code) const override;
	std::error_condition default_error_condition(int code) const noexcept override;
};

allio_detail_api
extern const error_category error_category_instance;

} // namespace detail

[[nodiscard]] inline std::error_category const& error_category()
{
	return detail::error_category_instance;
}


enum class error
{
	none,

	// Generic
	unknown_failure,
	invariant_violation,
	unsupported_operation,
	unsupported_input_format,
	device_or_resource_busy,

	// Arguments
	invalid_argument,
	argument_too_long,

	// Storage
	not_enough_memory,
	no_buffer_space,
	maximum_capacity_exceeded,

	// Encoding
	unsupported_encoding,
	invalid_encoding,

	// Handle
	handle_is_null,
	handle_is_not_null,
	handle_is_not_multiplexable,
	handle_cannot_be_detached,

	// Operations
	operation_pending,
	operation_canceled,
	operation_timed_out,

	// Byte I/O
	end_of_stream,
	io_size_out_of_range,

	// Filesystem
	filename_too_long,
	invalid_path,
	invalid_current_directory,
	unrepresentable_path,
	file_offset_out_of_range,

	// Memory
	invalid_address,
	unsupported_page_level,
	virtual_address_not_available,
	not_enough_address_space,

	// Process
	process_is_current_process,
	process_arguments_too_long,

	// Sockets
	socket_already_bound,
};

[[nodiscard]] inline std::error_code make_error_code(error const error)
{
	return std::error_code(static_cast<int>(error), detail::error_category_instance);
}

[[nodiscard]] inline std::error_condition make_error_condition(error const error)
{
	return std::error_condition(static_cast<int>(error), detail::error_category_instance);
}


enum class error_source : uintptr_t;

struct error_information
{
	std::error_code error;
	error_source source;
};

class error_handler
{
public:
	virtual void handle_error(error_information const& information) = 0;

protected:
	error_handler() = default;
	error_handler(error_handler const&) = default;
	error_handler& operator=(error_handler const&) = default;
	~error_handler() = default;
};


void set_error_handler(error_handler* const handler) noexcept;
[[nodiscard]] error_handler& get_error_handler() noexcept;

[[nodiscard]] inline error_handler& get_error_handler(error_handler* const handler) noexcept
{
	return handler != nullptr ? *handler : get_error_handler();
}

} // namespace allio

template<>
struct std::is_error_code_enum<allio::error>
{
	static constexpr bool value = true;
};
