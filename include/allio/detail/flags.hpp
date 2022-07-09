#pragma once

#define allio_detail_FLAG_ENUM_B(friend, T, O) \
	inline friend T operator O(T const l, T const r) \
	{ \
		using U = ::std::underlying_type_t<T>; \
		return static_cast<T>(static_cast<U>(l) O static_cast<U>(r)); \
	}

#define allio_detail_FLAG_ENUM_A(friend, T, O) \
	inline friend T& operator O ## =(T& l, T const r) \
	{ \
		using U = ::std::underlying_type_t<T>; \
		return l = static_cast<T>(static_cast<U>(l) O static_cast<U>(r)); \
	}

#define allio_detail_FLAG_ENUM_U(friend, T, O) \
	inline friend T operator O(T const r) \
	{ \
		using U = ::std::underlying_type_t<T>; \
		return static_cast<T>(O static_cast<U>(r)); \
	}

#define allio_detail_FLAG_ENUM_1(friend, T) \
	allio_detail_FLAG_ENUM_B(friend, T, &) \
	allio_detail_FLAG_ENUM_A(friend, T, &) \
	allio_detail_FLAG_ENUM_B(friend, T, |) \
	allio_detail_FLAG_ENUM_A(friend, T, |) \
	allio_detail_FLAG_ENUM_B(friend, T, ^) \
	allio_detail_FLAG_ENUM_A(friend, T, ^) \
	allio_detail_FLAG_ENUM_U(friend, T, ~) \

#define allio_detail_FLAG_ENUM(T) \
	allio_detail_FLAG_ENUM_1(, T)

#define allio_detail_FLAG_ENUM_FRIEND(T) \
	allio_detail_FLAG_ENUM_1(friend, T)
