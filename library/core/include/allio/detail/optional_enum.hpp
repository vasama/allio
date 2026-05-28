#pragma once

#include <vsm/concepts.hpp>
#include <vsm/utility.hpp>

namespace allio::detail {

template<bool IsEnumeration>
struct optional_enum_type;

template<>
struct optional_enum_type<0>
{
	template<typename T>
	using type = T;
};

template<>
struct optional_enum_type<1>
{
	template<typename T>
	using type = std::underlying_type_t<T>;
};

template<typename T>
class optional_enum
{
	using unsigned_type = typename optional_enum_type<std::is_enum_v<T>>::template type<T>;

	static_assert(std::is_unsigned_v<unsigned_type>);

	static constexpr unsigned_type null_value = static_cast<unsigned_type>(-1);

	unsigned_type m_value;

public:
	constexpr optional_enum()
		: m_value(null_value)
	{
	}

	constexpr optional_enum(T const value)
		: m_value(static_cast<unsigned_type>(value))
	{
		vsm_assert(static_cast<unsigned_type>(value) != null_value);
	}

	constexpr void operator->() const = delete;

	[[nodiscard]] constexpr T operator*() const
	{
		vsm_assert(m_value != null_value);
		return static_cast<T>(m_value);
	}

	[[nodiscard]] explicit constexpr operator bool() const noexcept
	{
		return m_value != null_value;
	}

	[[nodiscard]] constexpr bool has_value() const noexcept
	{
		return m_value != null_value;
	}

	template<std::convertible_to<T> U = T>
	[[nodiscard]] constexpr T value_or(U&& default_value) const
		noexcept(std::is_nothrow_convertible_v<U, T>)
	{
		if (m_value != null_value)
		{
			return static_cast<T>(m_value);
		}
		else
		{
			return vsm_forward(default_value);
		}
	}
};

} // namespace allio::detail
