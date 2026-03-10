#include <allio/detail/handles/file.hpp>

#include <allio/detail/serialization.hpp>
#include <allio/impl/handles/fs_object.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> file_t::open(
	native_handle<file_t>& h,
	io_parameters_t<file_t, open_t> const& a)
{
	return open_fs_object(h, open_kind::file, a);
}

vsm::result<void> file_t::serialize(
	native_handle<file_t>& h,
	serialization_context& serializer)
{
	vsm_try_void(serializer.visit_header("fil", 0));
	return serializer_visit(h, serializer);
}
