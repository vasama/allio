#pragma once

#include <vsm/assert.h>

#include <span>

#include <cstddef>
#include <cstdint>

namespace allio {
namespace detail {

template<typename U, typename T>
concept buffer_compatible = std::is_convertible_v<U(*)[], T(*)[]>;

} // namespace detail

class untyped_buffer
{
	void const* m_data;
	size_t m_size;

public:
	untyped_buffer()
		: m_data(nullptr)
		, m_size(0)
	{
	}

	untyped_buffer(void const* const data, size_t const size)
		: m_data(data)
		, m_size(size)
	{
	}

	bool empty() const
	{
		return m_size == 0;
	}

	size_t size() const
	{
		return m_size;
	}

	void const* data() const
	{
		return m_data;
	}
};

using untyped_buffers = std::span<const untyped_buffer>;

class untyped_buffers_storage
{
	static constexpr size_t view_mask = static_cast<size_t>(-1) >> 1;
	static constexpr size_t view_flag = ~view_mask;

	untyped_buffer m_buffer;

public:
	untyped_buffers_storage() = default;

	untyped_buffers_storage(untyped_buffer const buffer)
		: m_buffer(buffer.data(), buffer.size())
	{
		vsm_assert(buffer.size() <= view_mask);
	}

	untyped_buffers_storage(untyped_buffers const buffers)
		: m_buffer(buffers.data(), buffers.size() | view_flag)
	{
		vsm_assert(buffers.size() <= view_mask);
	}

	bool empty() const&
	{
		return m_buffer.size() == view_flag;
	}

	size_t size() const&
	{
		size_t const size = m_buffer.size();
		return size & view_flag ? size & view_mask : 1;
	}

	untyped_buffers buffers() const&
	{
		size_t const size = m_buffer.size();
		return size & view_flag
			? untyped_buffers(reinterpret_cast<untyped_buffer const*>(m_buffer.data()), size & view_mask)
			: untyped_buffers(&m_buffer, 1);
	}
};


template<typename T>
class basic_buffer : public untyped_buffer
{
	static_assert(sizeof(T) == 1);

public:
	basic_buffer() = default;

	basic_buffer(T* const data, size_t const size)
		: untyped_buffer(data, size)
	{
	}

	template<detail::buffer_compatible<T> U, size_t Size>
	basic_buffer(U(&data)[Size])
		: untyped_buffer(data, Size)
	{
	}

	template<detail::buffer_compatible<T> U>
	basic_buffer(std::span<U> const span)
		: untyped_buffer(span.data(), span.size())
	{
	}

	T* data() const
	{
		return static_cast<T*>(const_cast<void*>(untyped_buffer::data()));
	}

	size_t size() const
	{
		return untyped_buffer::size();
	}
};

template<typename T>
untyped_buffers as_untyped_buffers(std::span<basic_buffer<T>> const buffers)
{
	return untyped_buffers(static_cast<untyped_buffer const*>(buffers.data()), buffers.size());
}

template<typename T>
untyped_buffers as_untyped_buffers(std::span<const basic_buffer<T>> const buffers)
{
	return untyped_buffers(static_cast<untyped_buffer const*>(buffers.data()), buffers.size());
}

template<typename T>
using basic_buffers = std::span<const basic_buffer<T>>;


using read_buffer = basic_buffer<std::byte>;
using write_buffer = basic_buffer<const std::byte>;

using read_buffers = basic_buffers<std::byte>;
using write_buffers = basic_buffers<const std::byte>;

template<typename T>
basic_buffer<std::byte> as_read_buffer(std::span<T> const span)
{
	return basic_buffer<std::byte>(reinterpret_cast<std::byte*>(span.data()), span.size_bytes());
}

template<typename T>
basic_buffer<std::byte> as_read_buffer(T* const data, size_t const size)
{
	return basic_buffer<std::byte>(reinterpret_cast<std::byte*>(data), size * sizeof(T));
}

template<typename T>
basic_buffer<const std::byte> as_write_buffer(std::span<const T> const span)
{
	return basic_buffer<const std::byte>(reinterpret_cast<std::byte const*>(span.data()), span.size_bytes());
}

template<typename T>
basic_buffer<const std::byte> as_write_buffer(T const* const data, size_t const size)
{
	return basic_buffer<const std::byte>(reinterpret_cast<std::byte const*>(data), size * sizeof(T));
}

} // namespace allio
