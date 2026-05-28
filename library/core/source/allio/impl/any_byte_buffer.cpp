#include <allio/any_byte_buffer.hpp>

#include <allio/impl/error_encoding.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<std::span<std::byte>> any_byte_buffer_ref_base::_resize_byte_buffer(
	size_t const min_size,
	size_t const max_size) const
{
	size_t const size = m_bits >> min_object_alignment;

	if (min_size > static_cast<uintptr_t>(-1) << 1)
	{
		return vsm::unexpected(allio_error(error::io_size_out_of_range));
	}

	if (min_size > size)
	{
		return vsm::unexpected(allio_error(error::no_buffer_space));
	}

	return std::span<std::byte>(m_data, std::min(size, max_size));
}
