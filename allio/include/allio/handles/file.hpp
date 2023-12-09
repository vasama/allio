#pragma once

#include <allio/detail/handles/file.hpp>
#include <allio/handles/fs_object.hpp>

namespace allio {

using detail::file_t;
using detail::abstract_file_handle;

namespace blocking { using namespace detail::_file_b; }
namespace async { using namespace detail::_file_a; }

namespace blocking {

vsm::result<file_handle> open_null_file(auto&&... args);

vsm::result<file_handle> open_temporary_file(auto&&... args);

template<std::derived_from<detail::fs_object_t> FsObject>
vsm::result<file_handle> open_unique_file(detail::abstract_handle<FsObject> const& base, auto&&... args);

template<std::derived_from<detail::fs_object_t> FsObject>
vsm::result<file_handle> open_anonymous_file(detail::abstract_handle<FsObject> const& base, auto&&... args);

} // namespace blocking

} // namespace allio
