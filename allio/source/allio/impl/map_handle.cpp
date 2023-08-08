#include <allio/map_handle.hpp>

using namespace allio;
using namespace allio::detail;

bool map_handle_base::check_address_range(
	void const* const range_base, size_t const range_size)
{
	void const* const valid_beg = m_base.value;
	void const* const range_beg = range_base;

	void const* const valid_end = static_cast<char const*>(valid_beg) + m_size.value;
	void const* const range_end = static_cast<char const*>(range_beg) + range_size;

	return
		std::less_equal<>()(valid_beg, range_beg) &&
		std::less_equal<>()(range_end, valid_end);
}

