#pragma once

#include <vsm/type_traits.hpp>

#include <concepts>

namespace vsm {

/* type comparison */

template<typename T, typename U>
concept not_same_as = !std::same_as<T, U>;

template<typename T, typename... Ts>
concept any_of = is_any_of_v<T, Ts...>;


/* value category */

template<typename T>
concept non_cv = std::same_as<T, remove_cv_t<T>>;

template<typename T>
concept non_ref = std::same_as<T, remove_ref_t<T>>;

template<typename T>
concept non_cvref = std::same_as<T, remove_cvref_t<T>>;

template<typename T, typename U>
concept any_cv_of = std::same_as<remove_cv_t<T>, U>;

template<typename T, typename U>
concept any_ref_of = std::same_as<remove_ref_t<T>, U>;

template<typename T, typename U>
concept any_cvref_of = std::same_as<remove_cvref_t<T>, U>;

template<typename T, typename U>
concept no_cv_of = not_same_as<remove_cv_t<T>, U>;

template<typename T, typename U>
concept no_ref_of = not_same_as<remove_ref_t<T>, U>;

template<typename T, typename U>
concept no_cvref_of = not_same_as<remove_cvref_t<T>, U>;


/* type properties */

template<typename T>
concept enumeration = std::is_enum_v<T>;

template<typename T>
concept arithmetic = std::is_arithmetic_v<T>;

template<typename T>
concept character = std::integral<T> && any_of<T, char, wchar_t, char8_t, char16_t, char32_t>;

template<typename T>
concept inheritable = is_inheritable_v<T>;

template<typename T, template<typename...> typename Template>
concept instance_of = is_instance_of_v<T, Template>;

template<typename T, typename U>
concept constructible_to = std::constructible_from<U, T>;

} // namespace vsm
