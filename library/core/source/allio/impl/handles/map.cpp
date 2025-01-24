#include <allio/detail/handles/map.hpp>

#include <allio/impl/error_encoding.hpp>
#include <allio/nothrow/file.hpp>
#include <allio/nothrow/map.hpp>
#include <allio/nothrow/section.hpp>

#include <vsm/numeric.hpp>

using namespace allio;
using namespace allio::detail;

namespace io = nothrow::blocking;

//TODO: Deduplicate with other copies in platform-specific files.
static protection get_file_protection(native_handle<fs_object_t> const& h)
{
	protection protection = protection::none;
	if (h.flags[fs_object_t::flags::readable])
	{
		protection |= protection::read;
	}
	if (h.flags[fs_object_t::flags::writable])
	{
		protection |= protection::write;
	}
	return protection;
};

vsm::result<void> map_t::map_path(
	native_handle<map_t>& h,
	io_parameters_t<map_t, map_io::map_path_t> const& a)
{
	io::file_handle file;
	vsm_try_void(blocking_io<fs_io::open_t>(file, a));

	io_parameters_t<map_t, map_io::map_file_t> args = {};
	args.flags = a.flags;
	args.file = &file.native();
	args.protection = get_file_protection(file.native());

	return map_file(h, args);
}

vsm::result<void> map_t::map_file(
	native_handle<map_t>& h,
	io_parameters_t<map_t, map_io::map_file_t> const& a)
{
	if (a.file == nullptr)
	{
		return vsm::unexpected(allio_error(error::invalid_argument));
	}

	vsm_try(file_size, blocking_io<file_io::get_maximum_extent_t>(
		*a.file,
		file_io::get_maximum_extent_t::params_type()));

	vsm_try(mmap_size, vsm::try_truncate<size_t>(
		file_size,
		error::not_enough_address_space));

	io::section_handle section;
	{
		section_io::create_t::params_type args = {};
		args.options = section_options::backing_file;
		args.protection = a.protection;
		args.backing_storage = a.file;
		vsm_try_void(blocking_io<section_io::create_t>(section, args));
	}

	// Map the section:
	{
		map_io::map_memory_t::params_type args = {};
		args.options = map_options::backing_section | map_options::initial_commit;
		args.protection = a.protection;
		args.section = &section.native();
		args.size = mmap_size;
		return blocking_io<map_io::map_memory_t>(h, args);
	}
}
