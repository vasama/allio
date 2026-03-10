#pragma once

#include <allio/detail/handles/fs_object.hpp>

namespace allio::detail {

using fs_open_params_type = fs_io::open_t::params_type;
using open_parameters [[deprecated]] = fs_open_params_type;

enum class open_kind : uint8_t
{
	path,
	file,
	directory,
};

struct generic_open_options
{
	open_kind kind;
	file_mode mode;
	// file_options options;
	file_opening opening;
	file_sharing sharing;
	file_caching caching;
	open_options special;
	io_flags flags;
};

} // namespace allio::detail
