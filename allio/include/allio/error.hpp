#pragma once

#include <allio/detail/api.hpp>

#include <vsm/result.hpp>

namespace allio {
namespace detail {

struct error_category final : std::error_category
{
	char const* name() const noexcept override;
	std::string message(int const code) const override;
};

allio_detail_api extern const error_category error_category_instance;

} // namespace detail

inline std::error_category const& error_category()
{
	return detail::error_category_instance;
}


enum class error
{
	none,

	invalid_argument,
	not_enough_memory,
	no_buffer_space,
	filename_too_long,
	async_operation_cancelled,
	async_operation_not_in_progress,
	async_operation_timed_out,
	unsupported_operation,
	unsupported_multiplexer_handle_relation,
	unsupported_asynchronous_operation,
	too_many_concurrent_async_operations,
	handle_is_null,
	handle_is_not_null,
	handle_is_not_multiplexable,
	process_arguments_too_long,
	process_is_current_process,
	invalid_path,
	invalid_current_directory,
	directory_stream_at_end,
};

inline std::error_code make_error_code(error const error)
{
	return std::error_code(static_cast<int>(error), detail::error_category_instance);
}

} // namespace allio

template<>
struct std::is_error_code_enum<allio::error>
{
	static constexpr bool value = true;
};
