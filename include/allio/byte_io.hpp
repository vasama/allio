#pragma once

//#include <allio/detail/any_handle.hpp>
#include <allio/handle.hpp>
#include <allio/multiplexer.hpp>
#include <allio/type_list.hpp>

#include <span>

#include <cstddef>
#include <cstdint>

namespace allio {

using file_offset = uint64_t;

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


namespace detail {

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
		allio_ASSERT(buffer.size() <= view_mask);
	}

	untyped_buffers_storage(untyped_buffers const buffers)
		: m_buffer(buffers.data(), buffers.size() | view_flag)
	{
		allio_ASSERT(buffers.size() <= view_mask);
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

} // namespace detail

namespace io {

struct scatter_gather_parameters
	: basic_parameters
{
	using handle_type = handle const;
	using result_type = size_t;

	detail::untyped_buffers_storage buffers;
	file_offset offset;
};


struct scatter_read_at;
struct gather_write_at;

template<>
struct parameters<scatter_read_at> : scatter_gather_parameters
{
	parameters() = default;

	parameters(auto const& args, file_offset const offset, read_buffer const buffer)
		: scatter_gather_parameters{ args, buffer, offset }
	{
	}

	parameters(auto const& args, file_offset const offset, read_buffers const buffers)
		: scatter_gather_parameters{ args, as_untyped_buffers(buffers), offset }
	{
	}
};

template<>
struct parameters<gather_write_at> : scatter_gather_parameters
{
	parameters() = default;

	parameters(auto const& args, file_offset const offset, write_buffer const buffer)
		: scatter_gather_parameters{ args, buffer, offset }
	{
	}

	parameters(auto const& args, file_offset const offset, write_buffers const buffers)
		: scatter_gather_parameters{ args, as_untyped_buffers(buffers), offset }
	{
	}
};

using random_access_scatter_gather = type_list<
	scatter_read_at,
	gather_write_at
>;


struct stream_scatter_read;
struct stream_gather_write;

template<>
struct parameters<stream_scatter_read> : scatter_gather_parameters
{
	parameters() = default;

	parameters(auto const& args, read_buffer const buffer)
		: scatter_gather_parameters{ args, buffer, 0 }
	{
	}

	parameters(auto const& args, read_buffers const buffers)
		: scatter_gather_parameters{ args, as_untyped_buffers(buffers), 0 }
	{
	}
};

template<>
struct parameters<stream_gather_write> : scatter_gather_parameters
{
	parameters() = default;

	parameters(auto const& args, write_buffer const buffer)
		: scatter_gather_parameters{ args, buffer, 0 }
	{
	}

	parameters(auto const& args, write_buffers const buffers)
		: scatter_gather_parameters{ args, as_untyped_buffers(buffers), 0 }
	{
	}
};

using stream_scatter_gather = type_list<
	stream_scatter_read,
	stream_gather_write
>;

} // namespace io


#if 0
class stream_handle_ptr
{
public:
	using async_operations = io::stream_scatter_gather;

private:
	using table_type = detail::any_handle_operation_table<io::stream_scatter_gather>;

	handle const* m_handle;
	table_type const* m_table;

public:
	template<typename Handle>
	stream_handle_ptr(Handle const& handle)
		: m_handle(&handle)
		, m_table(&table_type::instance<typename Handle::async_operations>)
	{
	}


	multiplexer* get_multiplexer() const
	{
		return m_handle->get_multiplexer();
	}

	multiplexer_handle_relation const& get_multiplexer_relation() const
	{
		return m_handle->get_multiplexer_relation();
	}

	template<typename Operation>
	async_operation_descriptor const& get_descriptor()
	{
		return m_table->get_descriptor(
			m_handle->get_multiplexer_relation(),
			type_list_index<io::stream_scatter_gather, Operation>);
	}


	result<size_t> read(read_buffers const buffers) const
	{
		
	}

	result<size_t> write(write_buffers const buffers) const
	{

	}
};
#endif

} // namespace allio
