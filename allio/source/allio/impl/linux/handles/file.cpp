#include <allio/detail/handles/file.hpp>

#include <allio/impl/linux/api_string.hpp>
#include <allio/impl/linux/byte_io.hpp>
#include <allio/impl/linux/handles/fs_object.hpp>
#include <allio/linux/platform.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

vsm::result<void> file_t::open(
	native_type& h,
	io_parameters_t<file_t, open_t> const& a)
{
	auto const open_args = open_parameters::make(open_kind::file, a);
	vsm_try_bind((flags, mode), open_info::make(open_args));

	api_string_storage path_storage;
	vsm_try(path, make_api_c_string(path_storage, a.path.path.string()));

	vsm_try(fd, linux::open_file(
		unwrap_handle(a.path.base),
		path,
		flags,
		mode));

	h = fs_object_t::native_type
	{
		platform_object_t::native_type
		{
			object_t::native_type
			{
				flags::not_null | open_args.handle_flags,
			},
			wrap_handle(fd.release()),
		},
		//TODO: Validate flags
		a.flags,
	};

	return {};
}

vsm::result<size_t> file_t::random_read(
	native_type const& h,
	io_parameters_t<file_t, random_read_t> const& a)
{
	return linux::random_read(h, a);
}

vsm::result<size_t> file_t::random_write(
	native_type const& h,
	io_parameters_t<file_t, random_write_t> const& a)
{
	return linux::random_write(h, a);
}
