#pragma once

#include <allio/any_path_buffer.hpp>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

[[nodiscard]] vsm::result<size_t> read_link_path(
	int dirfd,
	char const* relative_path,
	string_buffer<char> buffer);

[[nodiscard]] vsm::result<size_t> read_link_path(
	int dirfd,
	char const* relative_path,
	any_path_buffer buffer);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
