#pragma once

#include <allio/byte_io.hpp>
#include <allio/platform_handle.hpp>

namespace allio {

vsm::result<size_t> sync_scatter_read(platform_handle const& h, io::scatter_gather_parameters const& args);
vsm::result<size_t> sync_gather_write(platform_handle const& h, io::scatter_gather_parameters const& args);

} // namespace allio
