#pragma once

#include <type_traits>

namespace allio::detail {

template<typename T, T Value = T{}>
struct linear
{
	static_assert(std::is_trivially_copyable_v<T>);

	T value = Value;

	linear() = default;

	linear(T const value) noexcept
		: value(value)
	{
	}

	linear(linear&& source) noexcept
		: value(source.value)
	{
		source.value = Value;
	}

	linear& operator=(linear&& source) noexcept
	{
		value = source.value;
		source.value = Value;
		return *this;
	}
};

} // namespace allio::detail
