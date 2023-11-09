#pragma once

#include <allio/filesystem.hpp> //TODO: Move to detail/

namespace allio {

//TODO: Constrain location to handle to fs_object_t
inline path_descriptor at(auto const& location, any_path_view const path)
{
	return path_descriptor(location, path);
}

} // namespace allio
