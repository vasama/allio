#pragma once

#include <allio/detail/byte_io_buffers.hpp>
#include <allio/impl/storage_provider.hpp>

namespace allio {

[[nodiscard]] vsm::result<detail::io_buffers_view> get_io_buffers(
	detail::io_buffers_base const& buffers,
	detail::io_buffer_layout required_layout,
	storage_provider_ref storage_provider);

[[nodiscard]] bool io_buffers_is_empty(detail::io_buffers_base buffers);
[[nodiscard]] size_t get_io_buffers_size(detail::io_buffers_base buffers);

} // namespace allio
