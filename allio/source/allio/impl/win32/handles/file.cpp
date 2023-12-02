#include <allio/detail/handles/file.hpp>

#include <allio/impl/win32/byte_io.hpp>
#include <allio/impl/win32/handles/fs_object.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<void> file_t::open(
	native_type& h,
	io_parameters_t<file_t, open_t> const& a)
{
	vsm_try_bind((file, flags), create_file(
		unwrap_handle(a.path.base),
		a.path.path,
		open_kind::file,
		a));

	h = platform_object_t::native_type
	{
		object_t::native_type
		{
			flags::not_null | flags,
		},
		wrap_handle(file.release()),
	};

	return {};
}

vsm::result<size_t> file_t::random_read(
	native_type const& h,
	io_parameters_t<file_t, random_read_t> const& a)
{
	return win32::random_read(h, a);
}

vsm::result<size_t> file_t::random_write(
	native_type const& h,
	io_parameters_t<file_t, random_write_t> const& a)
{
	return win32::random_write(h, a);
}
