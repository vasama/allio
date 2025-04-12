#include <allio/detail/handles/section.hpp>

#include <allio/detail/serialization.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> section_t::serialize(
	native_handle<section_t>& h,
	serialization_context& serializer)
{
	vsm_try_void(serializer.visit_header("sct", 0));
	return serializer_visit(h, serializer);
}
