#pragma once

#include <type_traits>

namespace vsm {
namespace detail::type_traits_ {

/* type comparison */

template<bool>
struct all_same;

template<>
struct all_same<0>
{
	template<typename...>
	static constexpr bool value = true;
};

template<>
struct all_same<1>
{
	template<typename T, typename... Ts>
	static constexpr bool value = (std::is_same_v<T, Ts> && ...);
};


/* conditional */

template<bool>
struct select;

template<>
struct select<0>
{
	template<typename, typename T>
	using type_template = T;
};

template<>
struct select<1>
{
	template<typename T, typename>
	using type_template = T;
};


/* value category copy */

template<typename T>
struct copy
{
	template<typename U>
	using cv = U;

	template<typename U>
	using ref = U;

	template<typename U>
	using cvref = U;
};

template<typename T>
struct copy<const T>
{
	template<typename U>
	using cv = const U;

	template<typename U>
	using ref = U;

	template<typename U>
	using cvref = U;
};

template<typename T>
struct copy<T&>
{
	template<typename U>
	using ref = U&;

	template<typename U>
	using cvref = U&;
};

template<typename T>
struct copy<const T&>
{
	template<typename U>
	using ref = U&;

	template<typename U>
	using cvref = const U&;
};

template<typename T>
struct copy<T&&>
{
	template<typename U>
	using ref = U&&;

	template<typename U>
	using cvref = U&&;
};

template<typename T>
struct copy<const T&&>
{
	template<typename U>
	using ref = const U;

	template<typename U>
	using cvref = const U&&;
};


/* type properties */

template<template<typename...> typename Template, typename T>
struct is_instance_of : std::false_type {};

template<template<typename...> typename Template, typename... Args>
struct is_instance_of<Template, Template<Args...>> : std::true_type {};

} // namespace detail::type_traits_

/* type comparison */

template<typename... Ts>
inline constexpr bool all_same_v = detail::type_traits_::all_same<(sizeof...(Ts) > 0)>::template X<Ts...>;

template<typename... Ts>
using all_same = std::bool_constant<all_same_v<Ts...>>;

template<typename T, typename... Ts>
inline constexpr bool is_any_of_v = (std::is_same_v<T, Ts> || ...);

template<typename... Ts>
using is_any_of = std::bool_constant<is_any_of_v<Ts...>>;


/* conditional */

template<bool Condition, typename True, typename False>
using select_t = typename detail::type_traits_::select<Condition>::template type_template<True, False>;


/* value category removal */

using std::remove_cv_t;
using std::remove_cvref_t;

template<typename T>
using remove_ref_t = std::remove_reference_t<T>;

template<typename T>
using remove_ptr_t = std::remove_pointer_t<T>;


/* value category copy */

template<typename T, typename U>
using copy_cv_t = typename detail::type_traits_::copy<T>::template cv<U>;

template<typename T, typename U>
using copy_ref_t = typename detail::type_traits_::copy<T>::template ref<U>;

template<typename T, typename U>
using copy_cvref_t = typename detail::type_traits_::copy<T>::template cvref<U>;


/* type properties */

template<typename T>
inline constexpr bool is_inheritable_v = std::is_class_v<T> && !std::is_final_v<T>;

template<typename T>
using is_inheritable = std::bool_constant<is_inheritable_v<T>>;

template<typename T, template<typename...> typename Template>
inline constexpr bool is_instance_of_v = detail::type_traits_::is_instance_of<Template, T>::value;

template<typename T, template<typename...> typename Template>
using is_instance_of = detail::type_traits_::is_instance_of<Template, T>;

} // namespace vsm
