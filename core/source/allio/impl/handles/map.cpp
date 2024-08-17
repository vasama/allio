#include <allio/detail/handles/map.hpp>

#include <allio/nothrow/blocking/file.hpp>
#include <allio/nothrow/blocking/map.hpp>
#include <allio/nothrow/blocking/section.hpp>

#include <vsm/numeric.hpp>

using namespace allio;
using namespace allio::detail;

namespace io = nothrow::blocking;

vsm::result<void> map_t::map_file(
	native_handle<map_t>& h,
	io_parameters_t<map_t, map_io::map_file_t> const& a)
{
	if (a.file == nullptr)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	vsm_try(file_size, blocking_io<file_io::get_maximum_extent_t>(
		*a.file,
		file_io::get_maximum_extent_t::params_type{}));

	vsm_try(mmap_size, vsm::try_truncate<size_t>(
		file_size,
		error::not_enough_address_space));

	section_io::create_t::params_type section_a = {};
	section_a.options = section_options::backing_file;
	section_a.protection = a.protection;
	section_a.backing_storage = a.file;

	io::section_handle section;
	vsm_try_void(blocking_io<section_io::create_t>(section, section_a));
	vsm_try(mapping, io::map_section(vsm_move(section), /* offset: */ 0, mmap_size));

	h = mapping.release();
	return {};
}

vsm::result<void> map_t::map_path(
	native_handle<map_t>& h,
	io_parameters_t<map_t, map_io::map_path_t> const& a)
{
	io::file_handle file;
	vsm_try_void(blocking_io<fs_io::open_t>(file, a));

	vsm_try(file_size, file.get_maximum_extent());
	vsm_try(mmap_size, vsm::try_truncate<size_t>(
		file_size,
		error::not_enough_address_space));

	vsm_try(section, io::create_section(vsm_move(file), mmap_size));
	vsm_try(mapping, io::map_section(vsm_move(section), /* offset: */ 0, mmap_size));

	h = mapping.release();
	return {};
}
