#include <allio/detail/handles/directory.hpp>

#include <allio/impl/handles/fs_object.hpp>
#include <allio/detail/serialization.hpp>
#include <allio/detail/uniplexer.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> directory_t::open(
	native_handle<directory_t>& h,
	io_parameters_t<directory_t, open_t> const& a)
{
	return open_fs_object(h, open_kind::directory, a);
}

vsm::result<void> directory_t::serialize(
	native_handle<directory_t>& h,
	serialization_context& serializer)
{
	vsm_try_void(serializer.visit_header("dir", 0));
	return serializer_visit(h, serializer);
}


vsm::result<bool> directory_iterator_t::next(
	native_handle<directory_iterator_t> const& h,
	io_parameters_t<directory_iterator_t, next_t> const& a)
{
	return uniplexer_handle::blocking_io<directory_iterator_t, next_t>(h, a);
}
