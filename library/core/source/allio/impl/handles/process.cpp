#include <allio/detail/handles/process.hpp>

#include <allio/detail/serialization.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> process_t::serializer_visit(
	native_handle<process_t>& h,
	serialization_context& serializer)
{
	vsm_try_void(base_type::serializer_visit(h, serializer));
	return serializer.visit_integer(h.id);
}

vsm::result<void> process_t::serialize(
	native_handle<process_t>& h,
	serialization_context& serializer)
{
	vsm_try_void(serializer.visit_header("pcs", 0));
	return serializer_visit(h, serializer);
}
