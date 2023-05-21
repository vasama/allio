#pragma once

#include <bit>

#ifndef __cpp_lib_byteswap
#	include <allio/detail/preprocessor.h>

#	include <concepts>

#	include <cstddef>
#	include <climits>

#	ifdef _MSC_VER
#		include <cstdlib>
#		define allio_detail_BSWAP(size, name) \
			allio_detail_CAT(_byteswap_, name)
#	else
#		define allio_detail_BSWAP(size, name) \
			allio_detail_CAT(__builtin_bswap, size)
#	endif
#endif

namespace allio::detail {

#ifdef __cpp_lib_byteswap
using std::byteswap;
#else
template<std::integral T>
constexpr T byteswap(T const value)
{
	using U = std::make_unsigned_t<T>;

	if (std::is_constant_evaluated())
	{
		U const m = static_cast<U>(static_cast<unsigned char>(-1));

		U v = static_cast<U>(value);
		U r = 0;

		for (size_t i = 0; i < sizeof(T); ++i)
		{
			r <<= CHAR_BIT;
			r |= v & m;
			v >>= CHAR_BIT;
		}

		return static_cast<T>(r);
	}
	else
	{
		if constexpr (sizeof(T) == 1)
		{
			return value;
		}
		else if constexpr (sizeof(T) == 2)
		{
			return static_cast<T>(allio_detail_BSWAP(16, ushort)(static_cast<U>(value)));
		}
		else if constexpr (sizeof(T) == 4)
		{
			return static_cast<T>(allio_detail_BSWAP(32, ulong)(static_cast<U>(value)));
		}
		else if constexpr (sizeof(T) == 8)
		{
			return static_cast<T>(allio_detail_BSWAP(64, uint64)(static_cast<U>(value)));
		}
		else
		{
			static_assert(sizeof(T) == 0);
		}
	}
}
#endif

} // namespace allio::detail
