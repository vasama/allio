#pragma once

#include <vsm/concepts.hpp>
#include <vsm/exceptions.hpp>
#include <vsm/standard/stdexcept.hpp>
#include <vsm/utility.hpp>

#include <optional>

namespace allio::detail {

template<bool IsEnumeration>
struct optional_flags_type;

template<>
struct optional_flags_type<0>
{
	template<typename T>
	using type = T;
};

template<>
struct optional_flags_type<1>
{
	template<typename T>
	using type = std::underlying_type_t<T>;
};

template<typename T>
class optional_flags
{
	using unsigned_type = typename optional_flags_type<std::is_enum_v<T>>::template type<T>;
	static_assert(std::is_unsigned_v<unsigned_type>);

	static constexpr unsigned_type flag = 1;
	static constexpr unsigned_type mask = static_cast<unsigned_type>(~flag);

	unsigned_type m_value;

public:
	constexpr optional_flags()
		: m_value(0)
	{
	}

	constexpr optional_flags(std::nullopt_t)
		: m_value(0)
	{
	}

	constexpr optional_flags(T const value)
		: m_value(static_cast<unsigned_type>(value))
	{
		vsm_assert((static_cast<unsigned_type>(value) & flag) == 0);
	}

	[[nodiscard]] constexpr void operator->() const = delete;

	[[nodiscard]] constexpr T operator*() const
	{
		vsm_assert(m_value & flag);
		return static_cast<T>(m_value & mask);
	}

	[[nodiscard]] explicit constexpr operator bool() const noexcept
	{
		return m_value & flag;
	}

	[[nodiscard]] constexpr bool has_value() const noexcept
	{
		return m_value & flag;
	}

	[[nodiscard]] constexpr T value() const
	{
		if (m_value & flag)
		{
			return static_cast<T>(m_value & mask);
		}
		else
		{
			vsm_except_throw(std::bad_optional_access());
		}
	}

	template<std::convertible_to<T> U = T>
	[[nodiscard]] constexpr T value_or(U&& default_value)
		noexcept(std::is_nothrow_convertible_v<U, T>)
	{
		if (m_value & flag)
		{
			return static_cast<T>(m_value & mask);
		}
		else
		{
			return vsm_forward(default_value);
		}
	}

	[[nodiscard]] constexpr T value_or_zero() const noexcept
	{
		return static_cast<T>(m_value & mask);
	}
};

} // namespace allio::detail
