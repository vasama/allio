#include <allio/platform_handle.hpp>

#include <vsm/utility.hpp>

using namespace allio;

bool platform_handle::check_native_handle(native_handle_type const& handle)
{
	return true
		&& base_type::check_native_handle(handle)
		&& handle.handle != native_platform_handle::null
	;
}

void platform_handle::set_native_handle(native_handle_type const& handle)
{
	base_type::set_native_handle(handle);
	m_native_handle.value = handle.handle;
}

platform_handle::native_handle_type platform_handle::release_native_handle()
{
	return
	{
		base_type::release_native_handle(),
		m_native_handle.release(),
	};
}
