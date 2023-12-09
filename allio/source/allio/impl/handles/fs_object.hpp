#pragma once

#include <allio/handles/fs_object.hpp>

namespace allio {

enum class open_kind : uint8_t
{
	file,
	directory,
};

struct open_parameters
{
	open_kind kind;
	bool inheritable;
	bool multiplexable;
	file_mode mode;
	file_creation creation;
	file_sharing sharing;
	file_caching caching;
	file_flags flags;

	static open_parameters make(
		open_kind const kind,
		detail::fs_io::open_t::optional_params_type const& args);
};

} // namespace allio
