#pragma once

#include <allio/file_handle.hpp>
#include <allio/memory.hpp>

namespace allio {

namespace io {

struct section_create;

} // namespace io

namespace detail {

class section_handle_base : public platform_handle
{
	using final_handle_type = final_handle<section_handle_base>;

public:
	using base_type = platform_handle;


	using create_parameters = basic_parameters;

#if 0
	#define allio_section_handle_create_parameters(type, data, ...) \
		type(allio::section_handle, create_parameters) \
		allio_platform_handle_create_parameters(__VA_ARGS__, __VA_ARGS__) \

	allio_interface_parameters(allio_section_handle_create_parameters);
#endif


	template<parameters<create_parameters> P = create_parameters::interface>
	vsm::result<void> create(file_size const maximum_size, P const& args = {})
	{
		return block_create(maximum_size, args);
	}

protected:
	using base_type::base_type;

private:
	vsm::result<void> block_create(file_size maximum_size, create_parameters const& args);

protected:
	using base_type::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::section_create> const& args);

	template<typename H, typename O>
	friend vsm::result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};

vsm::result<final_handle<section_handle_base>> block_create_section(
	file_size const maximum_size,
	section_handle_base::create_parameters const& args);

} // namespace detail

using section_handle = final_handle<detail::section_handle_base>;

template<parameters<section_handle::create_parameters> P = section_handle::create_parameters::interface>
vsm::result<section_handle> section_create(file_size const maximum_size, P const& args = {})
{
	return detail::block_create_section(maximum_size, args);
}

allio_detail_api extern allio_handle_implementation(section_handle);


template<>
struct io::parameters<io::section_create>
	: section_handle::create_parameters
{
	using handle_type = section_handle;
	using result_type = void;

	file_handle const* backing_file;
	file_size maximum_size;
};

} // namespace allio
