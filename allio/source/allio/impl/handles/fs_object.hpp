#pragma once

#include <allio/detail/handles/fs_object.hpp>

#include <allio/impl/handles/platform_object.hpp>

namespace allio::detail {
namespace open_kind {

inline constexpr open_options mask                  = open_options(3 << 6);
inline constexpr open_options path                  = open_options(1 << 6);
inline constexpr open_options file                  = open_options(2 << 6);
inline constexpr open_options directory             = open_options(3 << 6);

} // namespace open_kind

using open_parameters = fs_io::open_t::params_type;

vsm::result<handle_with_flags> open_file(open_parameters const& a);
vsm::result<handle_with_flags> open_unique_file(open_parameters const& a);

vsm::result<void> open_fs_object(
	fs_object_t::native_type& h,
	io_parameters_t<fs_object_t, fs_io::open_t> const& a,
	open_options kind);

} // namespace allio::detail
