#include <allio/detail/handles/thread.hpp>

using namespace
#include <allio/detail/serialization.hpp>
 allio;
using namespace allio::detail;

vsm::result<void> thread_t::serializer_visit(
	native_handle<thread_t>& h,
	serialization_context& context)
{
	vsm_try_void(base_type::serializer_visit(context, h));
	return context.visit(h.id);
}

vsm::result<void> thread_t::serialize(
	native_handle<thread_t>& h,
	serialization_context& serializer)
{
	vsm_try_void(serializer.visit_header("trd", 0));
	return serializer_visit(h, serializer);
}
