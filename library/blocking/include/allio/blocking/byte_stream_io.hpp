#pragma once

#include <allio/detail/byte_stream_io.hpp>

#include <vector>

namespace allio::blocking {

template<detail::handle Handle>
[[nodiscard]] size_t read_to_end(
	Handle const& handle,
	detail::any_byte_buffer const buffer,
	auto&&... args)
{
	return detail::throw_on_error(detail::_read_to_end_x(handle, buffer, vsm_forward(args)...));
}

template<typename Container = std::vector<std::byte>, detail::handle Handle>
[[nodiscard]] Container read_to_end(Handle const& handle, auto&&... args)
{
	return detail::throw_on_error(detail::_read_to_end_c<Container>(handle, vsm_forward(args)...));
}

} // namespace allio::blocking
