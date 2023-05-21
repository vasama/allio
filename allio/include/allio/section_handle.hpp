#pragma once

#include <allio/file_handle.hpp>

namespace allio {
namespace detail {

class section_handle_base : public platform_handle
{
public:
	vsm::result<void> create(file_size maximum_size);
	vsm::result<void> create(file_handle&& file, file_size maximum_size);
	vsm::result<void> create(file_handle const& file, file_size maximum_size);
};

} // namespace detail

using section_handle = final_handle<detail::section_handle_base>;
allio_detail_api extern allio_handle_implementation(section_handle);

} // namespace allio
