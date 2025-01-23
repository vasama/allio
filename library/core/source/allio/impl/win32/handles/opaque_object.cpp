#include <allio/detail/handles/opaque_object.hpp>

#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<void> opaque_object_t::poll(
	native_handle<opaque_object_t> const& h,
	io_parameters_t<opaque_object_t, poll_t> const& a)
{
	NTSTATUS const status = win32::NtWaitForSingleObject(
		reinterpret_cast<HANDLE>(h.object->handle_value),
		/* Alertable: */ false,
		kernel_timeout(a.deadline));

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

	allio_abi_result const result = h.object->functions->notify(
		h.object,
		/* information: */ 0);

	if (result != allio_abi_result_success)
	{
		return vsm::unexpected(result);
	}

	return {};
}
