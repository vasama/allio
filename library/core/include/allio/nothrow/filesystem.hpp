#pragma once

#include <allio/detail/fs_operations.hpp>
#include <allio/filesystem.hpp>
#include <allio/path.hpp>

namespace allio::nothrow {

template<typename Char, typename Encoding>
[[nodiscard]] vsm::result<basic_path_view<Char, Encoding>> copy_canonical(
	basic_path_view<Char, Encoding> const path,
	path_buffer<Char, Encoding> const buffer) noexcept
{
	return detail::canonical_path(path, buffer);
}

template<typename Path = path, typename Char, typename Encoding>
[[nodiscard]] vsm::result<Path> canonical(basic_path_view<Char, Encoding> const path) noexcept
{
	vsm::result<Path> r(vsm::result_value);
	if (auto const r2 = detail::canonical_path(path, *r); !r2)
	{
		r = vsm::unexpected(r2.error());
	}
	return r;
}

template<typename Char, typename Encoding>
[[nodiscard]] vsm::result<basic_path_view<Char, Encoding>> copy_weakly_canonical(
	basic_path_view<Char, Encoding> const path,
	path_buffer<Char, Encoding> const buffer) noexcept
{
	return detail::weakly_canonical_path(path, buffer);
}

template<typename Path = path, typename Char, typename Encoding>
[[nodiscard]] vsm::result<Path> weakly_canonical(
	basic_path_view<Char, Encoding> const path) noexcept
{
	vsm::result<Path> r(vsm::result_value);
	if (auto const r2 = detail::weakly_canonical_path(path, *r); !r2)
	{
		r = vsm::unexpected(r2.error());
	}
	return r;
}

template<typename Char, typename Encoding>
[[nodiscard]] vsm::result<basic_path_view<Char, Encoding>> copy_relative(
	basic_path_view<Char, Encoding> const path,
	fs_path const& base,
	path_buffer<Char, Encoding> const buffer) noexcept
{
	return detail::relative_path(path, base, buffer);
}

template<typename Path = path, typename Char, typename Encoding>
[[nodiscard]] vsm::result<Path> relative(
	basic_path_view<Char, Encoding> const path,
	fs_path const& base) noexcept
{
	vsm::result<Path> r(vsm::result_value);
	if (auto const r2 = detail::relative_path(path, base, *r); !r2)
	{
		r = vsm::unexpected(r2.error());
	}
	return r;
}

template<typename Char, typename Encoding>
[[nodiscard]] vsm::result<basic_path_view<Char, Encoding>> copy_proximate(
	basic_path_view<Char, Encoding> const path,
	fs_path const& base,
	path_buffer<Char, Encoding> const buffer) noexcept
{
	return detail::proximate_path(path, base, buffer);
}

template<typename Path = path, typename Char, typename Encoding>
[[nodiscard]] vsm::result<Path> proximate(
	basic_path_view<Char, Encoding> const path,
	fs_path const& base) noexcept
{
	vsm::result<Path> r(vsm::result_value);
	if (auto const r2 = detail::proximate_path(path, base, *r); !r2)
	{
		r = vsm::unexpected(r2.error());
	}
	return r;
}

} // namespace allio::nothrow
