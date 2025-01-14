#include <allio/detail/handles/opaque_object.hpp>

#include <allio/impl/linux/poll.hpp>

#include <vsm/numeric.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

vsm::result<void> opaque_object_t::poll(
	native_handle<opaque_object_t> const& h,
	io_parameters_t<opaque_object_t, poll_t> const& a)
{
	vsm_try(events, linux::poll(
		static_cast<int>(h.object->handle_value),
		static_cast<short>(h.object->object_flags),
		a.deadline));

	allio_abi_result const result = h.object->functions->notify(
		h.object,
		static_cast<uintptr_t>(events));

	if (result != allio_abi_result_success)
	{
		return vsm::unexpected(result);
	}

	return {};
}
