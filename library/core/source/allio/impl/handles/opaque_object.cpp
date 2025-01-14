#if 0 //TODO
#include <allio/detail/handles/opaque_object.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> opaque_object_t::close(
	native_handle<opaque_object_t>& h,
	io_parameters_t<opaque_object_t, close_t> const& a)
{
	h.object->functions->close(h.object);
	h.object = nullptr;

	return {};
}
#endif
