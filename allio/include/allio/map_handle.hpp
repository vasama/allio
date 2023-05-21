#pragma once

#include <allio/section_handle.hpp>

namespace allio {
namespace detail {

class map_handle_base
{
	vsm::linear<void*> m_base;
	vsm::linear<size_t> m_size;

public:
	[[nodiscard]] void* base() const
	{
		return m_base.value;
	}

	[[nodiscard]] size_t size() const
	{
		return m_size.value;
	}


	vsm::result<void> map(section_handle const& section, size_t size, basic_parameters::interface const& args = {});
};

} // namespace detail

using map_handle = final_handle<detail::map_handle_base>;

allio_detail_api extern allio_handle_implementation(map_handle);

} // namespace allio
