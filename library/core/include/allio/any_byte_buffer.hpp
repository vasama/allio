#pragma once

#include <allio/byte_io_buffers.hpp>
#include <allio/detail/mutable_buffer.hpp>

namespace allio::detail {

class any_byte_buffer
{
	static_assert(sizeof(uintptr_t) >= sizeof(size_t));

	using resize_t = vsm::result<read_buffer>(uintptr_t object, size_t min_size, size_t max_size);

	uintptr_t m_bits;

	union
	{
		std::byte* m_data;
		resize_t* m_resize;
	};

public:
	any_byte_buffer(read_buffer const buffer)
		: any_byte_buffer(buffer.data(), buffer.size())
	{
	}

	any_byte_buffer(std::byte* const data, size_t const size)
		: m_bits(static_cast<uintptr_t>(size) << 1 | 1)
		, m_data(data)
	{
	}

	template<mutable_contiguous_range Range>
	any_byte_buffer(Range&& range)
		: any_byte_buffer(std::ranges::data(range), std::ranges::size(range))
	{
	}

	template<mutable_contiguous_container Container>
		requires (alignof(Container) > 1)
	any_byte_buffer(Container& container)
		: any_byte_buffer(std::addressof(container), _resize_container<Container>)
	{
	}

	[[nodiscard]] vsm::result<read_buffer> resize(size_t const size) const
	{
		return resize(size, size);
	}

	[[nodiscard]] vsm::result<read_buffer> resize(
		size_t const min_size,
		size_t const max_size) const
	{
		vsm_assert(min_size <= max_size); //PRECONDITION

		if (m_bits & 1)
		{
			return _resize_borrowed(min_size, max_size);
		}
		else
		{
			return m_resize(m_bits, min_size, max_size);
		}
	}

private:
	template<typename Object>
	explicit any_byte_buffer(Object* const object, resize_t* const func)
		: m_bits(reinterpret_cast<uintptr_t>(object))
		, m_resize(func)
	{
	}

	template<typename Container>
	static vsm::result<read_buffer> _resize_container(
		uintptr_t const object,
		size_t const min_size,
		size_t const max_size)
	{
		auto& container = *reinterpret_cast<Container*>(object);
		vsm_try(size, detail::resize_container(container, min_size, max_size));
		return detail::as_read_buffer(std::span(container.data(), size));
	}

	vsm::result<read_buffer> _resize_borrowed(size_t min_size, size_t max_size) const;
};

} // namespace allio::detail

namespace allio {

using detail::any_byte_buffer;

} // namespace allio
