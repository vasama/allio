#pragma once

#include <allio/detail/api.hpp>

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
	not_enough_memory,
	no_buffer_space,
	filename_too_long,
	async_operation_cancelled,
	async_operation_not_in_progress,
	//TODO: Rename. Not necessarily asynchronous.
	async_operation_timed_out,
	unsupported_operation,
	unsupported_encoding,
	unsupported_multiplexer_handle_relation,
	unsupported_asynchronous_operation,
	unsupported_page_level,
	too_many_concurrent_async_operations,
	handle_is_null,
	handle_is_not_null,
	handle_is_not_multiplexable,
	process_arguments_too_long,
	process_is_current_process,
	invalid_path,
	invalid_current_directory,
	directory_stream_at_end,
	virtual_address_not_available,
	invalid_address,
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
