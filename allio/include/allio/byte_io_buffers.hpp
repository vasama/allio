#pragma once

#include <vsm/assert.h>
#include <vsm/concepts.hpp>

#include <span>

#include <cstddef>
#include <cstdint>

namespace allio {

template<vsm::any_cv_of<std::byte> T>
using basic_buffer = std::span<T>;

template<vsm::any_cv_of<std::byte> T>
using basic_buffers = std::span<basic_buffer<T> const>;

template<vsm::any_cv_of<std::byte> T>
class basic_buffers_storage
{
	using buffer_type = basic_buffer<T>;
	using buffers_type = basic_buffers<T>;

	static constexpr size_t view_mask = static_cast<size_t>(-1) >> 1;
	static constexpr size_t view_flag = ~view_mask;

	buffer_type m_buffer;

public:
	basic_buffers_storage() = default;

	basic_buffers_storage(buffer_type const buffer)
		: m_buffer(buffer)
	{
		vsm_assert(buffer.size() <= view_mask);
	}

	basic_buffers_storage(buffers_type const buffers)
		: m_buffer(const_cast<T*>(reinterpret_cast<T const*>(buffers.data())), buffers.size() | view_flag)
	{
		vsm_assert(buffers.size() <= view_mask);
	}

	[[nodiscard]] bool empty() const
	{
		return m_buffer.size() == view_flag;
	}

	[[nodiscard]] size_t size() const
	{
		size_t const size = m_buffer.size();
		return size & view_flag ? size & view_mask : 1;
	}

	[[nodiscard]] buffers_type buffers() const
	{
		size_t const size = m_buffer.size();
		return size & view_flag
			? buffers_type(reinterpret_cast<buffer_type const*>(m_buffer.data()), size & view_mask)
			: buffers_type(&m_buffer, 1);
	}
};


using read_buffer = basic_buffer<std::byte>;
using write_buffer = basic_buffer<std::byte const>;

using read_buffers = basic_buffers<std::byte>;
using write_buffers = basic_buffers<std::byte const>;

using read_buffers_storage = basic_buffers_storage<std::byte>;
using write_buffers_storage = basic_buffers_storage<std::byte const>;


template<typename T>
read_buffer as_read_buffer(std::span<T> const span)
{
	return read_buffer(reinterpret_cast<std::byte*>(span.data()), span.size_bytes());
}

template<typename T>
read_buffer as_read_buffer(T* const data, size_t const size)
{
	return read_buffer(reinterpret_cast<std::byte*>(data), size * sizeof(T));
}

template<typename T>
write_buffer as_write_buffer(std::span<T const> const span)
{
	return write_buffer(reinterpret_cast<std::byte const*>(span.data()), span.size_bytes());
}

template<typename T>
write_buffer as_write_buffer(T const* const data, size_t const size)
{
	return write_buffer(reinterpret_cast<std::byte const*>(data), size * sizeof(T));
}


} // namespace allio
