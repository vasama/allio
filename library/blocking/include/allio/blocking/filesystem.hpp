#pragma once

#include <allio/detail/exceptions.hpp>
#include <allio/detail/fs_operations.hpp>
#include <allio/filesystem.hpp>
#include <allio/path.hpp>

namespace allio::blocking {

template<typename Char, typename Encoding>
[[nodiscard]] basic_path_view<Char, Encoding> copy_canonical(
	basic_path_view<Char, Encoding> const path,
	path_buffer<Char, Encoding> const buffer)
{
	return detail::throw_on_error(detail::canonical_path(path, buffer))
}

template<typename Path = path, typename Char, typename Encoding>
[[nodiscard]] vsm::result<Path> canonical(basic_path_view<Char, Encoding> const path)
{
	Path r;
	if (auto const r2 = detail::canonical_path(path, r); !r2)
	{
		detail::throw_error(r2.error());
	}
	return r;
}

template<typename Char, typename Encoding>
[[nodiscard]] basic_path_view<Char, Encoding> copy_weakly_canonical(
	basic_path_view<Char, Encoding> const path,
	path_buffer<Char, Encoding> const buffer)
{
	return detail::throw_on_error(detail::weakly_canonical_path(path, buffer));
}

template<typename Path = path, typename Char, typename Encoding>
[[nodiscard]] vsm::result<Path> weakly_canonical(basic_path_view<Char, Encoding> const path)
{
	Path r;
	if (auto const r2 = detail::weakly_canonical_path(path, r); !r2)
	{
		detail::throw_error(r2.error());
	}
	return r;
}

template<typename Char, typename Encoding>
[[nodiscard]] basic_path_view<Char, Encoding> copy_relative(
	basic_path_view<Char, Encoding> const path,
	fs_path const& base,
	path_buffer<Char, Encoding> const buffer)
{
	return detail::throw_on_error(detail::relative_path(path, base, buffer));
}

template<typename Path = path, typename Char, typename Encoding>
[[nodiscard]] Path relative(basic_path_view<Char, Encoding> const path, fs_path const& base)
{
	Path r;
	if (auto const r2 = detail::relative_path(path, base, r); !r2)
	{
		detail::throw_error(r2.error());
	}
	return r;
}

template<typename Char, typename Encoding>
[[nodiscard]] basic_path_view<Char, Encoding> copy_proximate(
	basic_path_view<Char, Encoding> const path,
	fs_path const& base,
	path_buffer<Char, Encoding> const buffer)
{
	return detail::throw_on_error(detail::proximate_path(path, base, buffer));
}

template<typename Path = path, typename Char, typename Encoding>
[[nodiscard]] Path proximate(basic_path_view<Char, Encoding> const path, fs_path const& base)
{
	Path r;
	if (auto const r2 = detail::proximate_path(path, base, r); !r2)
	{
		detail::throw_error(r2.error());
	}
	return r;
}

} // namespace allio::blocking
