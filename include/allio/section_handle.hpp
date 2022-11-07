#pragma once

#include <allio/file_handle.hpp>

namespace allio {
namespace detail {

class section_handle_base : public platform_handle
{
public:
	result<void> create(file_offset maximum_size);
	result<void> create(file_handle&& file, file_offset maximum_size);
	result<void> create(file_handle const& file, file_offset maximum_size);
};

} // namespace detail

using section_handle = final_handle<detail::section_handle_base>;
allio_API extern allio_HANDLE_IMPLEMENTATION(section_handle);

} // namespace allio
