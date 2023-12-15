#pragma once

#include <allio/detail/api.hpp>

#include <vsm/concepts.hpp>
#include <vsm/result.hpp>

extern "C"
void allio_unrecoverable_error(std::error_code error);

namespace allio {
namespace detail {

void unrecoverable_error_default(std::error_code error);
void unrecoverable_error(std::error_code error);

inline void unrecoverable(vsm::result<void> const& e)
{
	if (!e)
	{
		unrecoverable_error(e.error());
	}
}

decltype(auto) unrecoverable(auto&& r, auto&& default_value)
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

inline std::error_category const& error_category()
{
	return detail::error_category_instance;
}


enum class error
{
	none,

	unknown_failure,
	invalid_argument,
	argument_string_too_long,
	not_enough_memory,
	no_buffer_space,
	filename_too_long,
	async_operation_pending,
	async_operation_cancelled,
	async_operation_not_in_progress,
	//TODO: Rename. Not necessarily asynchronous.
	async_operation_timed_out,
	unsupported_operation,
	invalid_encoding,
	unsupported_encoding,
	unsupported_multiplexer_handle_relation,
	unsupported_asynchronous_operation,
	unsupported_page_level,
	too_many_concurrent_async_operations,
	handle_is_null,
	handle_is_not_null,
	handle_is_not_multiplexable,
	multiplexer_is_null,
	process_arguments_too_long,
	process_is_current_process,
	process_exit_code_not_available,
	invalid_path,
	invalid_current_directory,
	directory_stream_at_end,
	virtual_address_not_available,
	invalid_address,
	command_line_too_long,
	socket_already_bound,
};

inline std::error_code make_error_code(error const error)
{
	return std::error_code(static_cast<int>(error), detail::error_category_instance);
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
error_handler& get_error_handler() noexcept;

inline error_handler& get_error_handler(error_handler* const handler) noexcept
{
	return handler != nullptr ? *handler : get_error_handler();
}

} // namespace allio

template<>
struct std::is_error_code_enum<allio::error>
{
	static constexpr bool value = true;
};
