#pragma once

#include <allio/detail/byte_io_buffers.hpp>

#include <vsm/arrow.hpp>

#include <cstring>

namespace allio::detail {

inline io_buffer read_io_buffer(void const* const src_buffer)
{
	io_buffer dst_buffer;
	std::memcpy(&dst_buffer, src_buffer, sizeof(io_buffer));
	return dst_buffer;
}

class io_buffer_iterator
{
	unsigned char const* m_ptr;

public:
	using value_type = io_buffer;
	using difference_type = ptrdiff_t;

	io_buffer_iterator() = default;

	explicit io_buffer_iterator(void const* const ptr)
		: m_ptr(static_cast<unsigned char const*>(ptr))
	{
	}

	[[nodiscard]] io_buffer operator*() const
	{
		return read_io_buffer(m_ptr);
	}

	[[nodiscard]] vsm::arrow<io_buffer> operator->() const
	{
		return { read_io_buffer(m_ptr) };
	}

	[[nodiscard]] io_buffer operator[](ptrdiff_t const offset) const
	{
		return read_io_buffer(m_ptr + offset * static_cast<ptrdiff_t>(sizeof(io_buffer)));
	}

	io_buffer_iterator& operator++() &
	{
		m_ptr += sizeof(io_buffer);
		return *this;
	}

	[[nodiscard]] io_buffer_iterator operator++(int) &
	{
		auto result = *this;
		m_ptr += sizeof(io_buffer);
		return result;
	}

	io_buffer_iterator& operator--() &
	{
		m_ptr -= sizeof(io_buffer);
		return *this;
	}

	[[nodiscard]] io_buffer_iterator operator--(int) &
	{
		auto result = *this;
		m_ptr -= sizeof(io_buffer);
		return result;
	}

	io_buffer_iterator& operator+=(ptrdiff_t const offset) &
	{
		m_ptr += offset * static_cast<ptrdiff_t>(sizeof(io_buffer));
		return *this;
	}

	io_buffer_iterator& operator-=(ptrdiff_t const offset) &
	{
		m_ptr -= offset * static_cast<ptrdiff_t>(sizeof(io_buffer));
		return *this;
	}

	[[nodiscard]] friend io_buffer_iterator operator+(
		io_buffer_iterator const& iterator,
		ptrdiff_t const offset)
	{
		return io_buffer_iterator(
			iterator.m_ptr + offset * static_cast<ptrdiff_t>(sizeof(io_buffer)));
	}

	[[nodiscard]] friend io_buffer_iterator operator+(
		ptrdiff_t const offset,
		io_buffer_iterator const& iterator)
	{
		return io_buffer_iterator(
			iterator.m_ptr + offset * static_cast<ptrdiff_t>(sizeof(io_buffer)));
	}

	[[nodiscard]] friend io_buffer_iterator operator-(
		io_buffer_iterator const& iterator,
		ptrdiff_t const offset)
	{
		return io_buffer_iterator(
			iterator.m_ptr - offset * static_cast<ptrdiff_t>(sizeof(io_buffer)));
	}

	[[nodiscard]] friend io_buffer_iterator operator-(
		ptrdiff_t const offset,
		io_buffer_iterator const& iterator)
	{
		return io_buffer_iterator(
			iterator.m_ptr - offset * static_cast<ptrdiff_t>(sizeof(io_buffer)));
	}

	[[nodiscard]] friend ptrdiff_t operator-(
		io_buffer_iterator const& lhs,
		io_buffer_iterator const& rhs)
	{
		return (lhs.m_ptr - rhs.m_ptr) / static_cast<ptrdiff_t>(sizeof(io_buffer));
	}

	[[nodiscard]] friend auto operator<=>(
		io_buffer_iterator const&,
		io_buffer_iterator const&) = default;
};
static_assert(std::random_access_iterator<io_buffer_iterator>);

using io_buffer_range = std::ranges::subrange<io_buffer_iterator>;

inline io_buffer_range read_io_buffers(io_buffers_view const& buffers)
{
	auto const data = static_cast<unsigned char const*>(buffers.buffers_data);

	return
	{
		io_buffer_iterator(data),
		io_buffer_iterator(data + buffers.buffers_size * sizeof(io_buffer)),
	};
}

template<vsm::any_cv_of<std::byte> T>
std::span<T> get_io_buffer_span(io_buffer const& buffer, io_buffer_layout const layout)
{
	bool const size_data = vsm::any_flags(layout, io_buffer_layout::size_data);

	auto const data = (size_data ? buffer.m1 : buffer.m0).data;
	auto const size = (size_data ? buffer.m0 : buffer.m1).size;

	return std::span<T>(static_cast<T*>(const_cast<void*>(data)), size);
}

} // namespace allio::detail
