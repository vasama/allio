#pragma once

#include <allio/error.hpp>

#include <vsm/allocator.hpp>
#include <vsm/assert.h>
#include <vsm/concepts.hpp>
#include <vsm/exceptions.hpp>
#include <vsm/memory.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

#include <bit>

#if 0
namespace allio::detail {

//TODO: Rename buffer to container

template<typename Container>
concept mutable_buffer = requires (Container container)
{
	requires vsm::non_cvref<typename std::remove_cvref_t<Container>::value_type>;
	{ container.data() } -> std::same_as<typename std::remove_cvref_t<Container>::value_type*>;
	{ container.size() } -> std::same_as<size_t>;
};


struct get_mutable_buffer_t
{
	template<mutable_buffer Container>
	[[nodiscard]] vsm_static_operator Container& operator()(
		Container& container) vsm_static_operator_const
	{
		return container;
	}

	template<typename Container>
		requires vsm::tag_invocable<get_mutable_buffer_t, Container&>
	[[nodiscard]] vsm_static_operator auto& operator()(
		Container& container) vsm_static_operator_const
	{
		static_assert(mutable_buffer<vsm::tag_invoke_result_t<get_mutable_buffer_t, Container&>>);
		return vsm::tag_invoke(get_mutable_buffer_t(), container);
	}
};
inline constexpr get_mutable_buffer_t get_mutable_buffer = {};

template<typename Container>
using mutable_buffer_from_t = decltype(get_mutable_buffer(std::declval<Container&>()));

template<typename Container>
concept indirectly_mutable_buffer = mutable_buffer<mutable_buffer_from_t<Container>>;


template<typename Container>
concept _resizable_container =
	mutable_buffer<Container> &&
	requires (Container container, size_t size)
	{
		{ container.capacity() } -> std::same_as<size_t>;
		{ container.resize(size) } -> std::same_as<void>;
	};

struct resize_buffer_t
{
#if 0
	template<typename Container>
	friend vsm::result<size_t> tag_invoke(
		resize_buffer_t,
		Container& container,
		size_t const min_size)
		requires requires { container.resize(min_size); }
	{
		return tag_invoke(resize_buffer_t(), container, min_size, min_size);
	}
#endif

	template<_resizable_container Container>
	friend vsm::result<size_t> tag_invoke(
		resize_buffer_t,
		Container& container,
		size_t const min_size,
		size_t const max_size) noexcept
	{
		vsm_except_try
		{
			if (container.size() < min_size)
			{
				container.resize(min_size);
			}

			if (min_size < max_size && container.size() < container.capacity())
			{
				// Resize the container further, up to min(max_size, capacity).
				container.resize(std::min(max_size, container.capacity()));
			}
		}
		vsm_except_catch (std::bad_alloc const&)
		{
			return vsm::unexpected(error::not_enough_memory);
		}

		return container.size();
	}

	template<typename StorageProvider>
		requires vsm::tag_invocable<resize_buffer_t, StorageProvider&, size_t>
	[[nodiscard]] vsm_static_operator vsm::result<size_t> operator()(
		StorageProvider& storage_provider,
		size_t const size) vsm_static_operator_const noexcept
	{
		if constexpr (vsm::tag_invocable<resize_buffer_t, StorageProvider&, size_t, size_t>)
		{
			return vsm::tag_invoke(resize_buffer_t(), storage_provider, size, size);
		}
		else
		{
			return vsm::tag_invoke(resize_buffer_t(), storage_provider, size);
		}
	}

	template<typename StorageProvider>
		requires vsm::tag_invocable<resize_buffer_t, StorageProvider&, size_t>
	[[nodiscard]] vsm_static_operator vsm::result<size_t> operator()(
		StorageProvider& storage_provider,
		size_t const min_size,
		size_t const max_size) vsm_static_operator_const noexcept
	{
		vsm_assert(min_size <= max_size); //PRECONDITION

		if constexpr (vsm::tag_invocable<resize_buffer_t, StorageProvider&, size_t, size_t>)
		{
			return vsm::tag_invoke(resize_buffer_t(), storage_provider, min_size, max_size);
		}
		else
		{
			return vsm::tag_invoke(resize_buffer_t(), storage_provider, min_size);
		}
	}
};
inline constexpr resize_buffer_t resize_buffer = {};

template<typename Container>
concept resizable_buffer =
	mutable_buffer<Container> &&
	requires (Container container, size_t const size)
	{
		detail::resize_buffer(container, size);
	};

template<typename Container>
concept indirectly_resizable_buffer = resizable_buffer<mutable_buffer_from_t<Container>>;

} // namespace allio::detail
#endif

namespace allio::detail {

template<typename Range>
concept mutable_contiguous_range =
	std::ranges::contiguous_range<Range> &&
	std::ranges::sized_range<Range> &&
	vsm::non_cv<std::ranges::range_value_t<Range>>;

template<typename Container>
concept mutable_contiguous_container =
	mutable_contiguous_range<Container> &&
	requires (Container container, size_t const size)
	{
		requires vsm::non_cvref<typename std::remove_cvref_t<Container>::value_type>;
		{ container.data() } -> std::same_as<typename std::remove_cvref_t<Container>::value_type*>;
		{ container.size() } -> std::same_as<size_t>;
		{ container.capacity() } -> std::same_as<size_t>;
		{ container.resize(size) } -> std::same_as<void>;
	};


struct get_mutable_range_t
{
	template<mutable_contiguous_range Range>
	[[nodiscard]] vsm_static_operator Range&& operator()(Range&& range) vsm_static_operator_const
	{
		return vsm_forward(range);
	}

	template<typename T>
		requires vsm::tag_invocable<get_mutable_range_t, T&>
	[[nodiscard]] vsm_static_operator mutable_contiguous_range auto& operator()(
		T& object) vsm_static_operator_const
	{
		return vsm::tag_invoke(get_mutable_range_t(), object);
	}
};
inline constexpr get_mutable_range_t get_mutable_range = {};

template<typename T>
using mutable_range_from_t = decltype(detail::get_mutable_range(std::declval<T&>()));

template<typename T>
concept indirectly_mutable_contiguous_range = requires { typename mutable_range_from_t<T>; };


struct resize_container_t
{
	template<mutable_contiguous_container Container>
	friend vsm::result<size_t> tag_invoke(
		resize_container_t,
		Container& container,
		size_t const min_size,
		size_t const max_size) noexcept
	{
		vsm_except_try
		{
			if (container.size() > max_size)
			{
				container.resize(max_size);
			}
			else
			{
				if (container.size() < min_size)
				{
					container.resize(min_size);
				}

				if (min_size < max_size && container.size() < container.capacity())
				{
					// Resize the container further, up to min(max_size, capacity).
					container.resize(std::min(max_size, container.capacity()));
				}
			}
		}
		vsm_except_catch (std::bad_alloc const&)
		{
			return vsm::unexpected(error::not_enough_memory);
		}

		return container.size();
	}

	template<typename Container>
		requires vsm::tag_invocable<resize_container_t, Container&, size_t, size_t>
	[[nodiscard]] vsm_static_operator vsm::result<size_t> operator()(
		Container& container,
		size_t const size) vsm_static_operator_const noexcept
	{
		if constexpr (vsm::tag_invocable<resize_container_t, Container&, size_t, size_t>)
		{
			return vsm::tag_invoke(resize_container_t(), container, size, size);
		}
		else
		{
			return vsm::tag_invoke(resize_container_t(), container, size);
		}
	}

	template<typename Container>
		requires vsm::tag_invocable<resize_container_t, Container&, size_t, size_t>
	[[nodiscard]] vsm_static_operator vsm::result<size_t> operator()(
		Container& container,
		size_t const min_size,
		size_t const max_size) vsm_static_operator_const noexcept
	{
		vsm_assert(min_size <= max_size); //PRECONDITION

		if constexpr (vsm::tag_invocable<resize_container_t, Container&, size_t, size_t>)
		{
			return vsm::tag_invoke(resize_container_t(), container, min_size, max_size);
		}
		else
		{
			return vsm::tag_invoke(resize_container_t(), container, min_size);
		}
	}
};
inline constexpr resize_container_t resize_container = {};

template<typename Container>
concept resizable_container =
	mutable_contiguous_range<Container> &&
	requires (Container container, size_t const size)
	{
		detail::resize_container(container, size, size);
	};


class get_storage_t
{
	static constexpr auto default_alignment = std::align_val_t(alignof(std::max_align_t));

public:
	template<resizable_container Container>
		requires vsm::byte_type<typename Container::value_type>
	friend vsm::result<vsm::allocation> tag_invoke(
		get_storage_t,
		Container& container,
		size_t const min_size,
		size_t const max_size,
		std::align_val_t const min_alignment_val) noexcept
	{
		using value_type = typename Container::value_type;
		size_t const min_alignment = static_cast<size_t>(min_alignment_val);

		size_t min_alloc_size = min_size;
		size_t max_alloc_size = min_size;

		while (container.size() < min_alloc_size)
		{
			vsm_try_discard(detail::resize_container(container, min_alloc_size, max_alloc_size));

			if (vsm::memalignment(container.data()) < min_alignment)
			{
				vsm_assert(min_alignment > alignof(value_type));
				min_alloc_size += min_alignment - alignof(value_type);
				min_alloc_size += min_alignment - alignof(value_type);
			}
		}

		uintptr_t const data_address = reinterpret_cast<uintptr_t>(container.data());
		size_t const alignment_offset = min_alignment - (data_address & min_alignment - 1);
		vsm_assert(container.size() - alignment_offset >= min_size);

		return vsm::allocation
		{
			reinterpret_cast<void*>(data_address + alignment_offset),
			container.size() - alignment_offset,
		};
	}

	template<typename T>
		requires vsm::tag_invocable<get_storage_t, T&, size_t, size_t, std::align_val_t>
	[[nodiscard]] vsm_static_operator vsm::result<vsm::allocation> operator()(
		T& object,
		size_t const size,
		std::align_val_t const min_alignment = default_alignment) vsm_static_operator_const noexcept
	{
		return operator()(object, size, size, min_alignment);
	}

	template<typename T>
		requires vsm::tag_invocable<get_storage_t, T&, size_t, size_t, std::align_val_t>
	[[nodiscard]] vsm_static_operator vsm::result<vsm::allocation> operator()(
		T& object,
		size_t const min_size,
		size_t const max_size,
		std::align_val_t const min_alignment = default_alignment) vsm_static_operator_const noexcept
	{
		vsm_assert(min_size <= max_size); //PRECONDITION
		vsm_assert(std::has_single_bit(static_cast<size_t>(min_alignment))); //PRECONDITION
		return vsm::tag_invoke(get_storage_t(), object, min_size, max_size, min_alignment);
	}
};
inline constexpr get_storage_t get_storage = {};

template<typename StorageProvider>
concept storage_provider = requires (StorageProvider& storage_provider, size_t const size)
{
	detail::get_storage(storage_provider, size);
};

} // namespace allio::detail
