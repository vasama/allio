#pragma once

#include <type_traits>

#define allio_detail_FLAG_ENUM_B(T, O, ...) \
	__VA_ARGS__ T operator O(T const l, T const r) \
	{ \
		using U = ::std::underlying_type_t<T>; \
		return static_cast<T>(static_cast<U>(l) O static_cast<U>(r)); \
	} \
	__VA_ARGS__ T& operator O ## =(T& l, T const r) \
	{ \
		using U = ::std::underlying_type_t<T>; \
		return l = static_cast<T>(static_cast<U>(l) O static_cast<U>(r)); \
	}

#define allio_detail_FLAG_ENUM_U(T, O, ...) \
	__VA_ARGS__ T operator O(T const r) \
	{ \
		using U = ::std::underlying_type_t<T>; \
		return static_cast<T>(O static_cast<U>(r)); \
	}

#define allio_detail_FLAG_ENUM_1(T, ...) \
	allio_detail_FLAG_ENUM_B(T, &, __VA_ARGS__) \
	allio_detail_FLAG_ENUM_B(T, |, __VA_ARGS__) \
	allio_detail_FLAG_ENUM_B(T, ^, __VA_ARGS__) \
	allio_detail_FLAG_ENUM_U(T, ~, __VA_ARGS__) \

#define allio_detail_FLAG_ENUM(T) \
	allio_detail_FLAG_ENUM_1(T, inline)

#define allio_detail_FLAG_ENUM_FRIEND(T) \
	allio_detail_FLAG_ENUM_1(T, friend)
