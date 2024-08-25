#pragma once

#include <allio/detail/byte_io_buffers.hpp>

#include <vsm/arrow.hpp>

namespace allio::detail {

inline new_io_buffer read_io_buffer(void const* const src_buffer)
{
	new_io_buffer dst_buffer;
	std::memcpy(&dst_buffer, src_buffer, sizeof(new_io_buffer));
	return dst_buffer;
}

class new_io_buffer_iterator
{
	unsigned char const* m_ptr;

public:
	using value_type = new_io_buffer;
	using difference_type = ptrdiff_t;

	new_io_buffer_iterator() = default;

	explicit new_io_buffer_iterator(void const* const ptr)
		: m_ptr(static_cast<unsigned char const*>(ptr))
	{
	}

	[[nodiscard]] new_io_buffer operator*() const
	{
		return read_io_buffer(m_ptr);
	}

	[[nodiscard]] vsm::arrow<new_io_buffer> operator->() const
	{
		return { read_io_buffer(m_ptr) };
	}

	[[nodiscard]] new_io_buffer operator[](ptrdiff_t const offset) const
	{
		return read_io_buffer(m_ptr + offset * static_cast<ptrdiff_t>(sizeof(new_io_buffer)));
	}

	new_io_buffer_iterator& operator++() &
	{
		m_ptr += sizeof(new_io_buffer);
		return *this;
	}

	[[nodiscard]] new_io_buffer_iterator operator++(int) &
	{
		auto result = *this;
		m_ptr += sizeof(new_io_buffer);
		return result;
	}

	new_io_buffer_iterator& operator--() &
	{
		m_ptr -= sizeof(new_io_buffer);
		return *this;
	}

	[[nodiscard]] new_io_buffer_iterator operator--(int) &
	{
		auto result = *this;
		m_ptr -= sizeof(new_io_buffer);
		return result;
	}

	new_io_buffer_iterator& operator+=(ptrdiff_t const offset) &
	{
		m_ptr += offset * static_cast<ptrdiff_t>(sizeof(new_io_buffer));
		return *this;
	}

	new_io_buffer_iterator& operator-=(ptrdiff_t const offset) &
	{
		m_ptr -= offset * static_cast<ptrdiff_t>(sizeof(new_io_buffer));
		return *this;
	}

	[[nodiscard]] friend new_io_buffer_iterator operator+(
		new_io_buffer_iterator const& iterator,
		ptrdiff_t const offset)
	{
		return new_io_buffer_iterator(
			iterator.m_ptr + offset * static_cast<ptrdiff_t>(sizeof(new_io_buffer)));
	}

	[[nodiscard]] friend new_io_buffer_iterator operator+(
		ptrdiff_t const offset,
		new_io_buffer_iterator const& iterator)
	{
		return new_io_buffer_iterator(
			iterator.m_ptr + offset * static_cast<ptrdiff_t>(sizeof(new_io_buffer)));
	}

	[[nodiscard]] friend new_io_buffer_iterator operator-(
		new_io_buffer_iterator const& iterator,
		ptrdiff_t const offset)
	{
		return new_io_buffer_iterator(
			iterator.m_ptr - offset * static_cast<ptrdiff_t>(sizeof(new_io_buffer)));
	}

	[[nodiscard]] friend new_io_buffer_iterator operator-(
		ptrdiff_t const offset,
		new_io_buffer_iterator const& iterator)
	{
		return new_io_buffer_iterator(
			iterator.m_ptr - offset * static_cast<ptrdiff_t>(sizeof(new_io_buffer)));
	}

	[[nodiscard]] friend ptrdiff_t operator-(
		new_io_buffer_iterator const& lhs,
		new_io_buffer_iterator const& rhs)
	{
		return (rhs.m_ptr - lhs.m_ptr) / sizeof(new_io_buffer);
	}

	[[nodiscard]] friend auto operator<=>(
		new_io_buffer_iterator const&,
		new_io_buffer_iterator const&) = default;
};
static_assert(std::random_access_iterator<new_io_buffer_iterator>);

using new_io_buffer_range = std::ranges::subrange<new_io_buffer_iterator>;

inline new_io_buffer_range read_io_buffers(new_io_buffers const& buffers)
{
	auto const data = static_cast<unsigned char const*>(buffers.buffers_data);

	return
	{
		new_io_buffer_iterator(data),
		new_io_buffer_iterator(data + buffers.buffers_size * sizeof(new_io_buffer)),
	};
}

template<vsm::any_cv_of<std::byte> T>
std::span<T> get_io_buffer_span(new_io_buffer const& buffer, new_io_buffer_layout const layout)
{
	bool const size_data = vsm::any_flags(layout, new_io_buffer_layout::size_data);

	auto const data = (size_data ? buffer.m1 : buffer.m0).data;
	auto const size = (size_data ? buffer.m0 : buffer.m1).size;

	return std::span<T>(static_cast<T*>(const_cast<void*>(data)), size);
}

} // namespace allio::detail
