#include <allio/detail/handles/directory.hpp>

#include <allio/impl/handles/fs_object.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> directory_t::open(
	native_type& h,
	io_parameters_t<directory_t, open_t> const& a)
{
	return open_fs_object(h, a, open_kind::directory);
}
