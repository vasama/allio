#pragma once

#include <type_traits>

namespace allio::detail {

template<typename T, typename Traits>
struct basic_linear
{
	static_assert(std::is_trivially_copyable_v<T>);

	T value = Traits::default_value();

	basic_linear() = default;

	basic_linear(T const value) noexcept
		: value(value)
	{
	}

	basic_linear(basic_linear&& source) noexcept
		: value(source.value)
	{
		source.value = Traits::default_value();
	}

	basic_linear& operator=(basic_linear&& source) noexcept
	{
		value = source.value;
		source.value = Traits::default_value();
		return *this;
	}

	void reset()
	{
		value = Traits::default_value();
	}

	T release()
	{
		T result = value;
		value = Traits::default_value();
		return result;
	}
};

template<typename T, T Value>
struct basic_linear_traits
{
	static constexpr T default_value()
	{
		return Value;
	}
};

template<typename T, T Value = T{}>
using linear = basic_linear<T, basic_linear_traits<T, Value>>;

} // namespace allio::detail
