#pragma once

#include <allio/detail/filesystem.hpp>

namespace allio::detail {

vsm::result<size_t> canonical_path(fs_path const& path, any_path_buffer buffer);
vsm::result<size_t> weakly_canonical_path(fs_path const& path, any_path_buffer buffer);
vsm::result<size_t> relative_path(fs_path const& path, fs_path const& base, any_path_buffer buffer);

vsm::result<size_t> proximate_path(
	fs_path const& path,
	fs_path const& base,
	any_path_buffer buffer);

template<typename Char, typename Encoding>
vsm::result<basic_path_view<Char, Encoding>> canonical_path(
	basic_path_view<Char, Encoding> path,
	path_buffer<Char, Encoding> buffer);

template<typename Char, typename Encoding>
vsm::result<basic_path_view<Char, Encoding>> weakly_canonical_path(
	basic_path_view<Char, Encoding> path,
	path_buffer<Char, Encoding> buffer);

template<typename Char, typename Encoding>
vsm::result<basic_path_view<Char, Encoding>> relative_path(
	basic_path_view<Char, Encoding> path,
	fs_path const& base,
	path_buffer<Char, Encoding> buffer);

template<typename Char, typename Encoding>
vsm::result<basic_path_view<Char, Encoding>> proximate_path(
	basic_path_view<Char, Encoding> path,
	fs_path const& base,
	path_buffer<Char, Encoding> buffer);

} // namespace allio::detail
