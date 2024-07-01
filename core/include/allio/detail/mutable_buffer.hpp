#pragma once

#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/concepts.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

namespace allio::detail {

void _mutable_buffer_ptr(auto) = delete;
void _mutable_buffer_ptr(auto*);
void _mutable_buffer_ptr(auto const*) = delete;

template<typename Container>
concept mutable_buffer = requires (Container& container)
{
	requires vsm::non_cvref<typename Container::value_type>;
	{ container.data() } -> std::same_as<typename Container::value_type*>;
	{ container.size() } -> std::convertible_to<size_t>;
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
		return vsm::tag_invoke(get_mutable_buffer_t(), container);
	}
};
inline constexpr get_mutable_buffer_t get_mutable_buffer = {};

struct resize_buffer_t
{
	template<typename Container>
	friend vsm::result<size_t> tag_invoke(
		resize_buffer_t,
		Container& container,
		size_t const min_size)
		requires requires { container.resize(min_size); }
	{
		return tag_invoke(resize_buffer_t(), container, min_size, min_size);
	}

	template<typename Container>
	friend vsm::result<size_t> tag_invoke(
		resize_buffer_t,
		Container& container,
		size_t const min_size,
		size_t const max_size)
		requires requires { container.resize(min_size); }
	{
		try
		{
			container.resize(min_size);

			if (min_size < max_size && container.size() < container.capacity())
			{
				// Resize the container further, up to min(max_size, capacity).
				container.resize(std::min(max_size, container.capacity()));
			}
		}
		catch (std::bad_alloc const&)
		{
			return vsm::unexpected(error::not_enough_memory);
		}
		return {};
	}

	template<typename Container>
		requires vsm::tag_invocable<resize_buffer_t, Container&, size_t>
	[[nodiscard]] vsm_static_operator vsm::result<size_t> operator()(
		Container& container,
		size_t const size) vsm_static_operator_const
	{
		return operator()(container, size, size);
	}

	template<typename Container>
		requires vsm::tag_invocable<resize_buffer_t, Container&, size_t>
	[[nodiscard]] vsm_static_operator vsm::result<size_t> operator()(
		Container& container,
		size_t const min_size,
		size_t const max_size) vsm_static_operator_const
	{
		vsm_assert(min_size <= max_size); //PRECONDITION
	
		if constexpr (vsm::tag_invocable<resize_buffer_t, Container&, size_t, size_t>)
		{
			return vsm::tag_invoke(resize_buffer_t(), container, min_size, max_size);
		}
		else
		{
			return vsm::tag_invoke(resize_buffer_t(), container, min_size);
		}
	}
};
inline constexpr resize_buffer_t resize_buffer = {};

template<typename Container>
concept resizable_buffer =
	mutable_buffer<Container> &&
	requires (Container& container, size_t const size)
	{
		detail::resize_buffer(container, size);
	};

} // namespace allio::detail
