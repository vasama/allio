#include <allio/map_handle.hpp>

#include <allio/impl/object_transaction.hpp>

#include <vsm/utility.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> map_handle_base::set_native_handle(native_handle_type const handle)
{
	object_transaction base_transaction(m_base.value, handle.base);
	object_transaction size_transaction(m_size.value, handle.size);
	vsm_try_void(base_type::set_native_handle(handle));
	base_transaction.commit();
	size_transaction.commit();

	return {};
}

vsm::result<map_handle_base::native_handle_type> map_handle_base::release_native_handle()
{
	vsm_try(base_handle, base_type::release_native_handle());

	return vsm::result<native_handle_type>
	{
		vsm::result_value,
		vsm_move(base_handle),
		std::exchange(m_base.value, nullptr),
		std::exchange(m_size.value, 0),
	};
}

bool map_handle_base::check_address_range(void const* const range_base, size_t const range_size) const
{
	void const* const valid_beg = m_base.value;
	void const* const range_beg = range_base;

	void const* const valid_end = static_cast<char const*>(valid_beg) + m_size.value;
	void const* const range_end = static_cast<char const*>(range_beg) + range_size;

	return
		std::less_equal<>()(valid_beg, range_beg) &&
		std::less_equal<>()(range_end, valid_end);
}
