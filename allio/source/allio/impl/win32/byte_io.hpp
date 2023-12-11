#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/platform_object.hpp>

namespace allio::win32 {

vsm::result<size_t> random_read(
	detail::platform_object_t::native_type const& h,
	detail::byte_io::random_parameters_t<std::byte> const& a);

vsm::result<size_t> random_write(
	detail::platform_object_t::native_type const& h,
	detail::byte_io::random_parameters_t<std::byte const> const& a);

vsm::result<size_t> stream_read(
	detail::platform_object_t::native_type const& h,
	detail::byte_io::stream_parameters_t<std::byte> const& a);

vsm::result<size_t> stream_write(
	detail::platform_object_t::native_type const& h,
	detail::byte_io::stream_parameters_t<std::byte const> const& a);

} // namespace allio::win32