#pragma once

#include <concepts>

namespace allio::detail {

template<typename Tag, std::integral Integer, Integer DefaultValue = 0>
class integer_id
{
	Integer m_integer = DefaultValue;

public:
	using integer_type = Integer;

	integer_id() = default;

	explicit constexpr integer_id(Integer const integer)
		: m_integer(integer)
	{
	}

	[[nodiscard]] constexpr Integer integer() const
	{
		return m_integer;
	}
};

} // namespace allio::detail
