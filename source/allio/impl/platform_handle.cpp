#include <allio/platform_handle.hpp>

#include <allio/impl/object_transaction.hpp>

using namespace allio;

result<void> platform_handle::set_native_handle(native_handle_type const handle)
{
	if (handle.handle == native_platform_handle::null)
	{
		return allio_ERROR(error::invalid_argument);
	}

	object_transaction handle_transaction(m_native_handle.value, handle.handle);
	allio_TRYV(base_type::set_native_handle(handle));
	handle_transaction.commit();

	return {};
}

result<platform_handle::native_handle_type> platform_handle::release_native_handle()
{
	allio_TRY(base_handle, base_type::release_native_handle());

#ifdef __clang__ //TODO: Remove this when clang is fixed...
	return native_handle_type
	{
		std::move(base_handle),
		std::exchange(m_native_handle.value, native_platform_handle::null),
	};
#else
	return result<native_handle_type>
	{
		result_value,
		std::move(base_handle),
		std::exchange(m_native_handle.value, native_platform_handle::null),
	};
#endif
}
