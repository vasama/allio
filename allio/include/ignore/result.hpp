#pragma once

#include <allio/detail/api.hpp>
#include <allio/detail/platform.hpp>
#include <allio/detail/preprocessor.h>

#if __has_include(<expected>)
#	include <expected>
#	define allio_detail_EXPECTED std
#else
#	include <tl/expected.hpp>
#	define allio_detail_EXPECTED tl
#endif

#include <system_error>
#include <type_traits>

namespace allio {

namespace detail {

struct error_category final : std::error_category
{
	char const* name() const noexcept override;
	std::string message(int const code) const override;
};

allio_api extern const error_category error_category_instance;

} // namespace detail

inline std::error_category const& error_category()
{
	return detail::error_category_instance;
}


template<typename T, typename E = std::error_code>
using result = allio_detail_EXPECTED::expected<T, E>;


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


#define allio_ERROR(...) \
	(::allio_detail_EXPECTED::unexpected(__VA_ARGS__))

inline constexpr auto& result_value = allio_detail_EXPECTED::in_place;
inline constexpr auto& result_error = allio_detail_EXPECTED::unexpect;

inline result<void> as_result(std::error_code const error)
{
	return error
		? result<void>{ result_error, error }
		: result<void>{};
}

inline std::error_code as_error_code(result<void> const& result)
{
	return result ? std::error_code() : result.error();
}

template<typename T, typename E>
result<void, E> discard_value(result<T, E> const& r)
{
	return r ? result<void, E>{} : result<void, E>(r.error());
}


#define allio_detail_TRY_IS_VOID(result) \
	(::std::is_void_v<typename ::std::remove_cvref_t<decltype(result)>::value_type>)

#define allio_detail_TRY_S0(_0, ...) \
	_0

#define allio_detail_TRY_S2(_0, _1, _2, ...) \
	_2

#define allio_detail_TRY_H(spec) \
	allio_detail_EXPAND(allio_detail_TRY_S0 allio_detail_EXPAND(allio_detail_TRY_S2 allio_detail_EXPAND((allio_detail_EXPAND spec, spec, (auto)))))

#define allio_detail_TRY_V(spec) \
	allio_detail_EXPAND(allio_detail_TRY_S2 allio_detail_EXPAND((, allio_detail_EXPAND spec, spec)))

#define allio_detail_TRY_R \
	allio_detail_CAT(allio_detail_TRY_r,allio_detail_COUNTER)

#define allio_detail_TRY_INTRO(return, wrap, hspec, result, ...) \
	hspec result = (__VA_ARGS__); \
	if (vsm_unlikely(!result)) \
	{ \
		return wrap(static_cast<decltype(result)&&>(result).error()); \
	}


#define allio_detail_TRYR_2(return, hspec, result, ...) \
	allio_detail_TRY_INTRO(return, allio_ERROR, hspec, result, __VA_ARGS__) \
	((void)0)

#define allio_detail_TRYR_1(return, spec, ...) \
	allio_detail_TRYR_2(return, allio_detail_TRY_H(spec), allio_detail_TRY_V(spec), __VA_ARGS__)

#define vsm_try_result(spec, ...) \
	allio_detail_TRYR_1(return, spec, __VA_ARGS__)

#define allio_CO_TRYR(spec, ...) \
	allio_detail_TRYR_1(co_return, spec, __VARGS__)


#define allio_detail_TRY_2(return, hspec, vspec, result, ...) \
	allio_detail_TRY_INTRO(return, allio_ERROR, hspec, result, __VA_ARGS__) \
	static_assert(!allio_detail_TRY_IS_VOID(result), "TRY requires a non-void result."); \
	auto&& vspec = *static_cast<decltype(result)&&>(result)

#define allio_detail_TRY_1(return, spec, ...) \
	allio_detail_TRY_2(return, allio_detail_TRY_H(spec), allio_detail_TRY_V(spec), allio_detail_TRY_R, __VA_ARGS__)

#define vsm_try(spec, ...) \
	allio_detail_TRY_1(return, spec, __VA_ARGS__)

#define allio_CO_TRY(spec, ...) \
	allio_detail_TRY_1(co_return, spec, __VA_ARGS__)


#define allio_detail_TRYA_2(return, hspec, left, result, ...) \
	allio_detail_TRY_INTRO(return, allio_ERROR, hspec, result, __VA_ARGS__) \
	static_assert(!allio_detail_TRY_IS_VOID(result), "TRYA requires a non-void result."); \
	((void)(left = *static_cast<decltype(result)&&>(result)))

#define allio_detail_TRYA_1(return, left, ...) \
	allio_detail_TRYA_2(return, allio_detail_TRY_H(left), allio_detail_TRY_V(left), allio_detail_TRY_R, __VA_ARGS__)

#define vsm_try_assign(left, ...) \
	allio_detail_TRYA_1(return, left, __VA_ARGS__)

#define allio_CO_TRYA(left, ...) \
	allio_detail_TRYA_1(co_return, left, __VA_ARGS__)


#define allio_detail_TRYS_2(return, hspec, vspec, result, ...) \
	allio_detail_TRY_INTRO(return, allio_ERROR, hspec, result, __VA_ARGS__) \
	static_assert(!allio_detail_TRY_IS_VOID(result), "TRYS requires a non-void result."); \
	auto&& [allio_detail_EXPAND vspec] = *static_cast<decltype(result)&&>(result)

#define allio_detail_TRYS_1(return, spec, ...) \
	allio_detail_TRYS_2(return, auto, spec, allio_detail_TRY_R, __VA_ARGS__)

#define allio_TRYS(spec, ...) \
	allio_detail_TRYS_1(return, spec, __VA_ARGS__)

#define allio_CO_TRYS(spec, ...) \
	allio_detail_TRYS_1(co_return, spec, __VA_ARGS__)


#define allio_detail_TRYV_2(return, hspec, result, ...) \
	allio_detail_TRY_INTRO(return, allio_ERROR, hspec, result, __VA_ARGS__) \
	static_assert(allio_detail_TRY_IS_VOID(result), "TRYV requires a void result."); \
	((void)result)

#define allio_detail_TRYV_1(return, ...) \
	allio_detail_TRYV_2(return, auto, allio_detail_TRY_R, __VA_ARGS__)

#define vsm_try_void(...) \
	allio_detail_TRYV_1(return, __VA_ARGS__)

#define allio_CO_TRYV(...) \
	allio_detail_TRYV_1(co_return, __VA_ARGS__)


#define allio_detail_TRYD_2(return, hspec, result, ...) \
	allio_detail_TRY_INTRO(return, allio_ERROR, hspec, result, __VA_ARGS__) \
	((void)result)

#define allio_detail_TRYD_1(return, ...) \
	allio_detail_TRYD_2(return, auto, allio_detail_TRY_R, __VA_ARGS__)

#define vsm_try_discard(...) \
	allio_detail_TRYD_1(return, __VA_ARGS__)

#define allio_CO_TRYD(...) \
	allio_detail_TRYD_1(co_return, __VA_ARGS__)


#define allio_detail_TRYE_2(return, hspec, result, ...) \
	allio_detail_TRY_INTRO(return, ::std::error_code, hspec, result, __VA_ARGS__) \
	static_assert(allio_detail_TRY_IS_VOID(result), "TRYV requires a void result."); \
	((void)result)

#define allio_detail_TRYE_1(return, left, ...) \
	allio_detail_TRYE_2(return, auto, allio_detail_TRY_R, __VA_ARGS__)

#define allio_TRYE(...) \
	allio_detail_TRYE_1(return, __VA_ARGS__)

#define allio_CO_TRYE(...) \
	allio_detail_TRYE_1(co_return, __VA_ARGS__)

} // namespace allio

template<>
struct std::is_error_code_enum<allio::error>
{
	static constexpr bool value = true;
};
