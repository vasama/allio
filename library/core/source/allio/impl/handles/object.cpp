#include <allio/detail/object.hpp>

#include <allio/detail/serialization.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> object_t::serializer_visit(
	native_handle<object_t>& h,
	serialization_context& context)
{
	return context.visit_integer(h.flags);
}
