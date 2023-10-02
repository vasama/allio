#pragma once

#include <allio/detail/handles/map_handle.hpp>

namespace allio {

using map_handle = basic_blocking_handle<detail::_map_handle>;


template<parameters<map_handle::basic_map_parameters> P = map_handle::basic_map_parameters>
vsm::result<map_handle> map_section(section_handle const& section, file_size const offset, size_t const size, P const& args = {});

template<parameters<map_handle::basic_map_parameters> P = map_handle::basic_map_parameters>
vsm::result<map_handle> map_anonymous(size_t const size, P const& args = {});

} // namespace allio
