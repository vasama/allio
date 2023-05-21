#include <allio/platform_handle.hpp>

#include <allio/impl/object_transaction.hpp>

#include <vsm/utility.hpp>

using namespace allio;

vsm::result<void> platform_handle::set_native_handle(native_handle_type const handle)
{
	if (handle.handle == native_platform_handle::null)
	{
		return std::unexpected(error::invalid_argument);
	}

	object_transaction handle_transaction(m_native_handle.value, handle.handle);
	vsm_try_void(base_type::set_native_handle(handle));
	handle_transaction.commit();

	return {};
}

vsm::result<platform_handle::native_handle_type> platform_handle::release_native_handle()
{
	vsm_try(base_handle, base_type::release_native_handle());

	return vsm::result<native_handle_type>
	{
		vsm::result_value,
		vsm_move(base_handle),
		std::exchange(m_native_handle.value, native_platform_handle::null),
	};
}
