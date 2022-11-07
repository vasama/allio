#pragma once

#include <allio/detail/api.hpp>
#include <allio/filesystem_handle.hpp>
#include <allio/multiplexer.hpp>

namespace allio {

namespace detail {

class path_handle_base : public filesystem_handle
{
};

} // namespace detail

using path_handle = final_handle<detail::path_handle_base>;

result<path_handle> open_path(path_view path);
result<path_handle> open_path(filesystem_handle const& base, path_view path);

allio_API extern allio_HANDLE_IMPLEMENTATION(path_handle);

} // namespace allio
