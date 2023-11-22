#pragma once

#include <vsm/tag_invoke.hpp>

#include <concepts>
#include <span>
#include <string_view>

#include <cstdint>

namespace allio::detail {

enum class option_tree_type_enum : uintptr_t
{
	null,
	boolean,
	sint8,
	uint8,
	sint16,
	uint16,
	sint32,
	uint32,
	sint64,
	uint64,
	string,
	bytes_r,
	bytes_w,
	array,
	table,
};

class alignas(2) option_tree_type_info
{
	uint8_t m_guid[16];
};

class option_tree_type
{
	static_assert(alignof(option_tree_type_info) > 1);

	uintptr_t m_type;

public:
	option_tree_type(option_tree_type_enum const type)
		: m_type(static_cast<uintptr_t>(type) << 1 | 1)
	{
	}

	option_tree_type(option_tree_type_info const& type)
		: m_type(reinterpret_cast<uintptr_t>(&type))
	{
	}
};

struct option_tree_pair;

template<size_t Size>
class option_tree_array;

template<size_t Size>
class option_tree_table;

class option_tree_value
{
	option_tree_type m_type;
	union
	{
		struct
		{
			void const* m_data;
			size_t m_size;
		};
		uint64_t m_integer;
	};

public:
	option_tree_value(bool const boolean)
		: m_type(option_tree_type_enum::boolean)
		, m_integer(boolean)
	{
	}

	option_tree_value(std::integral auto const integer)
		: m_type(option_tree_type_enum::uint64) //TODO
		, m_integer(static_cast<uint64_t>(integer))
	{
		static_assert(integer <= sizeof(m_integer));
	}

	option_tree_value(std::string_view const string)
		: m_type(option_tree_type_enum::string)
		, m_data(string.data())
		, m_size(string.size())
	{
	}

	option_tree_value(std::span<std::byte> const bytes)
		: m_type(option_tree_type_enum::bytes_w)
		, m_data(bytes.data())
		, m_size(bytes.size())
	{
	}

	option_tree_value(std::span<std::byte const> const bytes)
		: m_type(option_tree_type_enum::bytes_w)
		, m_data(const_cast<std::byte*>(bytes.data()))
		, m_size(bytes.size())
	{
	}

	template<size_t Size>
	option_tree_value(option_tree_array<Size> const& array)
		: m_type(option_tree_type_enum::array)
		, m_data(array.data())
		, m_size(array.size())
	{
	}

	option_tree_value(std::span<option_tree_value const> array)
		: m_type(option_tree_type_enum::array)
		, m_data(array.data())
		, m_size(array.size())
	{
	}

	template<size_t Size>
	option_tree_value(option_tree_table<Size> const& table)
		: m_type(option_tree_type_enum::table)
		, m_data(table.data())
		, m_size(table.size())
	{
	}

	option_tree_value(std::span<option_tree_pair const> table)
		: m_type(option_tree_type_enum::table)
		, m_data(table.data())
		, m_size(table.size())
	{
	}
};

struct option_tree_pair
{
	std::string_view key;
	option_tree_value value;
};

template<size_t Size>
class option_tree_array
{
	option_tree_value m_array[Size];

public:
	option_tree_array(auto const&... args)
		: m_array{ args... }
	{
	}

	option_tree_value const* data() const
	{
		return m_array;
	}

	size_t size() const
	{
		return Size;
	}
};

template<typename... Args>
option_tree_array(Args const&...) -> option_tree_array<sizeof...(Args)>;

template<size_t Size>
class option_tree_table
{
	option_tree_pair m_table[Size];

public:
	option_tree_table(auto const&... args)
		: m_table{ args... }
	{
	}

	option_tree_pair const* data() const
	{
		return m_table;
	}

	size_t size() const
	{
		return Size;
	}
};

template<typename... Args>
option_tree_table(Args const&...) -> option_tree_table<sizeof...(Args)>;

using option_tree_view = std::span<option_tree_pair const>;


struct make_option_t
{
	option_tree_pair vsm_static_operator_invoke(option_tree_pair const& option)
	{
		return option;
	}

	template<typename Value>
	option_tree_pair vsm_static_operator_invoke(Value const& value)
		requires vsm::tag_invocable<make_option_t, Value const&>
	{
		return vsm::tag_invoke(make_option_t(), value);
	}
};
inline constexpr make_option_t make_option = {};

} // namespace allio::detail
