#pragma once

#include <allio/detail/new.hpp>
#include <allio/error.hpp>

#include <vsm/lazy.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/utility.hpp>

#include <memory>

namespace allio {

struct unique_ptr_deleter
{
	template<typename T>
	void vsm_static_operator_invoke(T* const object)
	{
		object->~T();

		detail::memory_release(
			object,
			sizeof(T),
			alignof(T),
			/* automatic: */ false);
	}
};

template<typename T>
using unique_ptr = std::unique_ptr<T, unique_ptr_deleter>;

template<typename T>
vsm::result<unique_ptr<T>> make_unique2(auto&&... args)
{
	auto const allocation = detail::memory_acquire(
		/* min_size: */ sizeof(T),
		/* max_size: */ sizeof(T),
		alignof(T),
		/* automatic: */ false);

	if (allocation.memory == nullptr)
	{
		return vsm::unexpected(error::not_enough_memory);
	}

	T* const object = new (allocation.memory) T(vsm_forward(args)...);
	return vsm::result<unique_ptr<T>>(vsm::result_value, object);
}


namespace detail {

struct operator_deleter
{
	void vsm_static_operator_invoke(void* const memory)
	{
		operator delete(memory);
	}
};

} // namespace detail

template<typename T = void>
vsm::result<std::unique_ptr<T, detail::operator_deleter>> allocate_unique(size_t size)
{
	if constexpr (!std::is_void_v<T>)
	{
		size = size * sizeof(T);
	}

	T* const ptr = operator new(size, std::nothrow);

	if (ptr == nullptr)
	{
		return vsm::unexpected(error::not_enough_memory);
	}

	return vsm_lazy(std::unique_ptr<T, detail::operator_deleter>(ptr));
}


template<typename T>
	requires (!std::is_array_v<T>)
constexpr vsm::result<std::unique_ptr<T>> make_unique(auto&&... args)
{
	T* const ptr = new (std::nothrow) T(vsm_forward(args)...);

	if (ptr == nullptr)
	{
		return vsm::unexpected(error::not_enough_memory);
	}

	return vsm_lazy(std::unique_ptr<T>(ptr));
}

template<typename T>
	requires std::is_unbounded_array_v<T>
constexpr vsm::result<std::unique_ptr<T>> make_unique(size_t const size)
{
	T* const ptr = new (std::nothrow) T[size]{};

	if (ptr == nullptr)
	{
		return vsm::unexpected(error::not_enough_memory);
	}

	return vsm_lazy(std::unique_ptr<T>(ptr));
}

template<typename T>
	requires std::is_bounded_array_v<T>
void make_unique(auto&&... args) = delete;

} // namespace allio
