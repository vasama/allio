#include <allio/detail/handles/event.hpp>

#include <allio/detail/serialization.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> event_t::serialize(
	native_handle<event_t>& h,
	serialization_context& serializer)
{
	vsm_try_void(serializer.visit_header("evt", 0));
	return serializer_visit(h, serializer);
}
