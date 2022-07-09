#include <allio/platform_handle.hpp>

#include "object_transaction.hpp"

using namespace allio;

result<void> platform_handle::set_native_handle(native_handle_type const handle)
{
	if (handle.handle == native_platform_handle::null)
	{
		return allio_ERROR(make_error_code(std::errc::bad_file_descriptor));
	}
	if (m_native_handle.value != native_platform_handle::null)
	{
		return allio_ERROR(error::handle_is_not_null);
	}

	object_transaction handle_transaction(m_native_handle.value, handle.handle);
	allio_TRYV(handle::set_native_handle(handle));
	handle_transaction.commit();
	return {};
}

result<platform_handle::native_handle_type> platform_handle::release_native_handle()
{
	if (m_native_handle.value == native_platform_handle::null)
	{
		return allio_ERROR(make_error_code(std::errc::bad_file_descriptor));
	}

	allio_TRY(base_handle, handle::release_native_handle());
#ifdef __clang__ //TODO: Remove this when clang is fixed...
	return native_handle_type
	{
		std::move(base_handle),
		std::exchange(m_native_handle.value, native_platform_handle::null)
	};
#else
	return
	{
		result_value,
		std::move(base_handle),
		std::exchange(m_native_handle.value, native_platform_handle::null),
	};
#endif
}

result<void> platform_handle::do_close()
{
	allio_ASSERT(*this);
	if (multiplexer* const multiplexer = get_multiplexer())
	{
		if (!is_synchronous<io::close>(*this))
		{
			return block<io::close>(*this);
		}
	}
	return do_close_sync();
}
