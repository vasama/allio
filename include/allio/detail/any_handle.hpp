#pragma once

#include <allio/type_list.hpp>

#include <cstdint>

namespace allio::detail {

template<typename Operations>
struct any_handle_operation_table;

template<typename... Operations>
class any_handle_operation_table<type_list<Operations...>>
{
	using operations = type_list<Operations...>;

	uint8_t m_handle_operation_indices[sizeof...(Operations)];

public:
	async_operation_descriptor const& get_descriptor(multiplexer_handle_relation const& relation, size_t const operation_index) const
	{
		return relation.operations[m_handle_operation_indices[operation_index]];
	}
};

} // namespace allio::detail
