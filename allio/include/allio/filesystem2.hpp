#pragma once

#include <allio/filesystem.hpp> //TODO: Move to detail/

namespace allio {

template<std::derived_from<fs_object_t> FsObject>
path_descriptor at(detail::abstract_handle<FsObject> const& location, any_path_view const path)
{
	return path_descriptor(location, path);
}

} // namespace allio
