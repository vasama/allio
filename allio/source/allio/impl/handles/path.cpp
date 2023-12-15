#include <allio/detail/handles/path.hpp>

#include <allio/impl/handles/fs_object.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> path_t::open(
	native_type& h,
	io_parameters_t<path_t, open_t> const& a)
{
	return open_fs_object(h, a, open_kind::path);
}
