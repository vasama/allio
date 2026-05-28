#pragma once

#include <allio/detail/mutable_buffer.hpp>

#include <vsm/function_ptr.hpp>
#include <vsm/standard/memory.hpp>

#include <span>

namespace allio::detail {

class any_byte_buffer_ref;

class any_byte_buffer_ref_base
{
	static_assert(sizeof(uintptr_t) >= sizeof(size_t));

	static constexpr size_t min_object_alignment        = 4;

	static constexpr uintptr_t mode_mask                = min_object_alignment - 1;
	static constexpr uintptr_t pointer_mask             = ~mode_mask;

	static constexpr size_t mode_byte_buffer            = 0;
	static constexpr size_t mode_byte_container         = 1;
	static constexpr size_t mode_user_defined           = 2;

	using resize_byte_container_t = vsm::result<std::span<std::byte>>(
		void* object,
		size_t min_size,
		size_t max_size);

	uintptr_t m_bits;

	union
	{
		std::byte* m_data;
		resize_byte_container_t* m_resize_byte_container;
		vsm::function_ptr_t m_user_defined;
	};

public:
	any_byte_buffer_ref_base() noexcept
		: m_bits(0)
	{
	}

	any_byte_buffer_ref_base(std::span<std::byte> const buffer)
		: any_byte_buffer_ref_base(buffer.data(), buffer.size())
	{
	}

	any_byte_buffer_ref_base(std::byte* const data, size_t const size)
		: m_bits(static_cast<uintptr_t>(size) << min_object_alignment)
		, m_data(data)
	{
	}

	template<mutable_contiguous_range Range>
	any_byte_buffer_ref_base(Range&& range)
		: any_byte_buffer_ref_base(std::ranges::data(range), std::ranges::size(range))
	{
	}

	template<mutable_contiguous_container Container>
		requires (alignof(Container) >= min_object_alignment)
	any_byte_buffer_ref_base(Container& container)
		: m_bits(reinterpret_cast<uintptr_t>(std::addressof(container)) | mode_byte_container)
		, m_resize_byte_container(_resize_byte_container<Container>)
	{
	}

	[[nodiscard]] vsm::result<std::span<std::byte>> resize(size_t const size) const
	{
		return resize(size, size);
	}

	[[nodiscard]] vsm::result<std::span<std::byte>> resize(
		size_t const min_size,
		size_t const max_size) const
	{
		vsm_assert(min_size <= max_size); //PRECONDITION
		vsm_assert(!is_user_defined()); //PRECONDITION

		if ((m_bits & mode_mask) == mode_byte_buffer)
		{
			return _resize_byte_buffer(min_size, max_size);
		}
		else
		{
			return m_resize_byte_container(object_pointer(), min_size, max_size);
		}
	}

protected:
	explicit any_byte_buffer_ref_base(void* const object, vsm::function_ptr_t const function)
		: m_bits(reinterpret_cast<uintptr_t>(object) | mode_user_defined)
		, m_user_defined(function)
	{
		vsm_assert(vsm::is_sufficiently_aligned<min_object_alignment>(object)); //PRECONDITION
	}

	[[nodiscard]] bool is_user_defined() const noexcept
	{
		return (m_bits & mode_mask) == mode_user_defined;
	}

	[[nodiscard]] any_byte_buffer_ref as_byte_buffer() const;

	[[nodiscard]] void* object_pointer() const noexcept
	{
		return reinterpret_cast<void*>(m_bits & pointer_mask);
	}

	[[nodiscard]] vsm::function_ptr_t function_pointer() const noexcept
	{
		return m_user_defined;
	}

private:
	template<typename Container>
	static vsm::result<std::span<std::byte>> _resize_byte_container(
		void* const object,
		size_t const min_size,
		size_t const max_size)
	{
		auto& container = *static_cast<Container*>(object);
		vsm_try(size, detail::resize_container(container, min_size, max_size));
		return std::as_writable_bytes(std::span(container.data(), size));
	}

	vsm::result<std::span<std::byte>> _resize_byte_buffer(size_t min_size, size_t max_size) const;
};

class any_byte_buffer_ref : any_byte_buffer_ref_base
{
public:
	using any_byte_buffer_ref_base::any_byte_buffer_ref_base;

	using any_byte_buffer_ref_base::resize;

private:
	explicit any_byte_buffer_ref(any_byte_buffer_ref_base const& other)
		: any_byte_buffer_ref_base(other)
	{
	}

	friend any_byte_buffer_ref_base;
};

inline any_byte_buffer_ref any_byte_buffer_ref_base::as_byte_buffer() const
{
	vsm_assert(!is_user_defined()); //PRECONDITION
	return any_byte_buffer_ref(*this);
}

#if 0
class any_byte_buffer
{
	static_assert(sizeof(uintptr_t) >= sizeof(size_t));

	using resize_t = vsm::result<std::span<std::byte>>(
		uintptr_t object,
		size_t min_size,
		size_t max_size);

	uintptr_t m_bits;

	union
	{
		std::byte* m_data;
		resize_t* m_resize;
	};

public:
	any_byte_buffer(std::span<std::byte> const buffer)
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

	[[nodiscard]] vsm::result<std::span<std::byte>> resize(size_t const size) const
	{
		return resize(size, size);
	}

	[[nodiscard]] vsm::result<std::span<std::byte>> resize(
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
	static vsm::result<std::span<std::byte>> _resize_container(
		uintptr_t const object,
		size_t const min_size,
		size_t const max_size)
	{
		auto& container = *reinterpret_cast<Container*>(object);
		vsm_try(size, detail::resize_container(container, min_size, max_size));
		return std::as_writable_bytes(std::span(container.data(), size));
	}

	vsm::result<std::span<std::byte>> _resize_borrowed(size_t min_size, size_t max_size) const;
};
#endif

// TODO: Get rid of one of the names.
using any_byte_buffer = any_byte_buffer_ref;

} // namespace allio::detail

namespace allio {

using detail::any_byte_buffer_ref;
using detail::any_byte_buffer;

} // namespace allio
