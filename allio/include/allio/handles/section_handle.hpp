#pragma once

#include <allio/detail/handles/section_handle.hpp>

namespace allio {

using section_handle = basic_handle<detail::_section_handle>;


template<parameters<section_handle::create_parameters> P = section_handle::create_parameters::interface>
vsm::result<section_handle> create_section(file_size const maximum_size, P const& args = {});

} // namespace allio
