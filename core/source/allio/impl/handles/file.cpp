#include <allio/detail/handles/file.hpp>

#include <allio/impl/handles/fs_object.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> file_t::open(
	native_type& h,
	io_parameters_t<file_t, open_t> const& a)
{
	return open_fs_object(h, a, open_kind::file);
}
