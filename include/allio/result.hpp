#pragma once

#include <allio/detail/api.hpp>
#include <allio/detail/platform.hpp>
#include <allio/detail/preprocessor.h>

#include <tl/expected.hpp>

#include <system_error>
#include <type_traits>

namespace allio {

namespace detail {

struct error_category final : std::error_category
{
	char const* name() const noexcept override;
	std::string message(int const code) const override;
};

allio_API extern const error_category error_category_instance;

} // namespace detail

inline std::error_category const& error_category()
{
	return detail::error_category_instance;
}


template<typename T, typename E = std::error_code>
using result = tl::expected<T, E>;


enum class error
{
	async_operation_cancelled,
	unsupported_multiplexer_handle_relation,
	too_many_concurrent_async_operations,
	unsupported_asynchronous_operation,
	handle_is_not_null,
	handle_is_not_multiplexable,
};

inline std::error_code make_error_code(error const error)
{
	return std::error_code(static_cast<int>(error), detail::error_category_instance);
}


#define allio_ERROR(...) \
	(::tl::make_unexpected(__VA_ARGS__))

inline constexpr auto& result_value = tl::in_place;
inline constexpr auto& result_error = tl::unexpect;

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

#define allio_detail_TRY_INTRO(return, hspec, result, ...) \
	hspec result = (__VA_ARGS__); \
	if (allio_detail_UNLIKELY(!result)) \
	{ \
		return allio_ERROR(static_cast<decltype(result)&&>(result).error()); \
	}


#define allio_detail_TRY_2(return, hspec, vspec, result, ...) \
	allio_detail_TRY_INTRO(return, hspec, result, __VA_ARGS__) \
	static_assert(!allio_detail_TRY_IS_VOID(result), "TRY requires a non-void result."); \
	auto&& vspec = *static_cast<decltype(result)&&>(result)

#define allio_detail_TRY_1(return, spec, ...) \
	allio_detail_TRY_2(return, allio_detail_TRY_H(spec), allio_detail_TRY_V(spec), allio_detail_TRY_R, __VA_ARGS__)

#define allio_TRY(spec, ...) \
	allio_detail_TRY_1(return, spec, __VA_ARGS__)

#define allio_CO_TRY(spec, ...) \
	allio_detail_TRY_1(co_return, spec, __VA_ARGS__)


#define allio_detail_TRYA_2(return, hspec, left, result, ...) \
	allio_detail_TRY_INTRO(return, hspec, result, __VA_ARGS__) \
	static_assert(!allio_detail_TRY_IS_VOID(result), "TRYA requires a non-void result."); \
	((void)(left = *static_cast<decltype(result)&&>(result)))

#define allio_detail_TRYA_1(return, left, ...) \
	allio_detail_TRYA_2(return, allio_detail_TRY_H(left), allio_detail_TRY_V(left), allio_detail_TRY_R, __VA_ARGS__)

#define allio_TRYA(left, ...) \
	allio_detail_TRYA_1(return, left, __VA_ARGS__)

#define allio_CO_TRYA(left, ...) \
	allio_detail_TRYA_1(co_return, left, __VA_ARGS__)


#define allio_detail_TRYS_2(return, hspec, vspec, result, ...) \
	allio_detail_TRY_INTRO(return, hspec, result, __VA_ARGS__) \
	static_assert(!allio_detail_TRY_IS_VOID(result), "TRYS requires a non-void result."); \
	auto&& [allio_detail_EXPAND vspec] = *static_cast<decltype(result)&&>(result)

#define allio_detail_TRYS_1(return, spec, ...) \
	allio_detail_TRYS_2(return, auto, spec, allio_detail_TRY_R, __VA_ARGS__)

#define allio_TRYS(spec, ...) \
	allio_detail_TRYS_1(return, spec, __VA_ARGS__)

#define allio_CO_TRYS(spec, ...) \
	allio_detail_TRYS_1(co_return, spec, __VA_ARGS__)


#define allio_detail_TRYV_2(return, hspec, result, ...) \
	allio_detail_TRY_INTRO(return, hspec, result, __VA_ARGS__) \
	static_assert(allio_detail_TRY_IS_VOID(result), "TRYV requires a void result."); \
	((void)result)

#define allio_detail_TRYV_1(return, ...) \
	allio_detail_TRYV_2(return, auto, allio_detail_TRY_R, __VA_ARGS__)

#define allio_TRYV(...) \
	allio_detail_TRYV_1(return, __VA_ARGS__)

#define allio_CO_TRYV(...) \
	allio_detail_TRYV_1(co_return, __VA_ARGS__)


#define allio_detail_TRYD_2(return, hspec, result, ...) \
	allio_detail_TRY_INTRO(return, hspec, result, __VA_ARGS__) \
	((void)result)

#define allio_detail_TRYD_1(return, ...) \
	allio_detail_TRYD_2(return, auto, allio_detail_TRY_R, __VA_ARGS__)

#define allio_TRYD(...) \
	allio_detail_TRYD_1(return, __VA_ARGS__)

#define allio_CO_TRYD(...) \
	allio_detail_TRYD_1(co_return, __VA_ARGS__)

} // namespace allio

template<>
struct std::is_error_code_enum<allio::error>
{
	static constexpr bool value = true;
};
