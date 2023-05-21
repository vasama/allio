#pragma once

namespace allio::detail {

struct string_ref
{
	// String size exceeded maximum.
	static constexpr size_t size_flag = ~(static_cast<size_t>(-1) >> 1);
	static constexpr size_t cstr_flag = size_flag >> 1;
	static constexpr size_t wide_flag = size_flag >> 2;

	static constexpr size_t size_mask = static_cast<size_t>(-1) >> 3;

	void const* m_data;
	size_t m_size;
};

} // namespace allio::detail
