#include <allio/detail/handles/platform_object.hpp>

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/linux/error.hpp>

#include <unistd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

vsm::result<void> platform_object_t::close(
	native_type& h,
	io_parameters_t<platform_object_t, close_t> const&)
{
	if (h.platform_handle != native_platform_handle::null)
	{
		unrecoverable(close_handle(h.platform_handle));
	}
	h = {};
	return {};
}
