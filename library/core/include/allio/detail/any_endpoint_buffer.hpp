#pragma once

#include <allio/error.hpp>
#include <allio/detail/mutable_buffer.hpp>
#include <allio/network.hpp>

#include <vsm/result.hpp>
#include <vsm/tag_ptr.hpp>

namespace allio::detail {

class any_endpoint_buffer
{
	static constexpr size_t min_static_alignment = 4;

	enum class tag_type
	{
		byte_span,
		container,
		typed_ptr,
	};

	using resize_container_t = vsm::result<vsm::allocation>(
		void* object,
		size_t min_size,
		size_t max_size,
		std::align_val_t min_alignment);

	vsm::incomplete_tag_ptr<void, tag_type, tag_type::typed_ptr> m_data;

	union
	{
		size_t m_size;
		network_address_kind m_kind;
		resize_container_t* m_resize_container;
	};

public:
	any_endpoint_buffer()
		: m_data(nullptr)
		, m_size(0)
	{
	}

	explicit any_endpoint_buffer(void* const data, size_t const size)
		: any_endpoint_buffer(data, size, vsm::memalignment(data) >= min_static_alignment)
	{
		vsm_assert(data != nullptr || size == 0); //PRECONDITION
	}

	template<mutable_contiguous_range Range>
		requires vsm::byte_type<std::ranges::range_value_t<Range>>
	explicit(!_platform_endpoint<Range>)
	any_endpoint_buffer(Range&& range)
		: any_endpoint_buffer(std::ranges::data(range), std::ranges::size(range))
	{
	}

	template<resizable_container Container>
		requires vsm::byte_type<typename Container::value_type>
	explicit(!_platform_endpoint<Container>)
	any_endpoint_buffer(Container& container)
		: m_data(std::addressof(container), tag_type::container)
		, m_resize_container(_resize_container<Container>)
	{
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_data != nullptr || m_size != 0;
	}

	[[nodiscard]] network_address_kind kind() const
	{
		return m_data.tag() == tag_type::typed_ptr
			? m_kind
			: network_address_kind::null;
	}

	[[nodiscard]] bool is_platform_endpoint() const
	{
		return m_data.tag() != tag_type::typed_ptr;
	}

	[[nodiscard]] vsm::result<vsm::allocation> resize(
		size_t const min_size,
		size_t const max_size,
		std::align_val_t const min_alignment) const
	{
		vsm_assert(is_platform_endpoint()); //PRECONDITION

		if (m_data.tag() == tag_type::container)
		{
			return m_resize_container(m_data.ptr(), min_size, max_size, min_alignment);
		}

		if (m_size < min_size)
		{
			return vsm::unexpected(error::no_buffer_space);
		}

		if (vsm::memalignment(m_data.ptr()) < static_cast<size_t>(min_alignment))
		{
			return vsm::unexpected(error::insufficient_alignment);
		}

		return vsm::allocation(m_data.ptr(), m_size);
	}

private:
	explicit any_endpoint_buffer(
		void* const data,
		size_t const size,
		bool const is_sufficiently_aligned)
		: m_data(is_sufficiently_aligned ? data : nullptr)
		, m_size(is_sufficiently_aligned ? size : static_cast<size_t>(-1))
	{
	}

	template<typename Container>
	vsm::result<vsm::allocation> _resize_container(
		void* const object,
		size_t const min_size,
		size_t const max_size,
		std::align_val_t const min_alignment)
	{
		auto& container = *static_cast<Container*>(object);
		vsm_try(container_size, resize_container(container, min_size, max_size));

		if (vsm::memalignment(container.data()) < static_cast<size_t>(min_alignment))
		{
			return vsm::unexpected(error::insufficient_alignment);
		}

		return vsm::allocation(container.data(), container_size);
	}
};

} // namespace allio::detail
