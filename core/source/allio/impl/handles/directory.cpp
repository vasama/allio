#include <allio/detail/handles/directory.hpp>

#include <allio/impl/handles/fs_object.hpp>
#include <allio/detail/uniplexer.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> directory_t::open(
	native_handle<directory_t>& h,
	io_parameters_t<directory_t, open_t> const& a)
{
	return open_fs_object(h, a, open_kind::directory);
}

vsm::result<bool> directory_iterator_t::next(
	native_handle<directory_iterator_t> const& h,
	io_parameters_t<directory_iterator_t, next_t> const& a)
{
	return uniplexer_handle::blocking_io<directory_iterator_t, next_t>(h, a);
}
