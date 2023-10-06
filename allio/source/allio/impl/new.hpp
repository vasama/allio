#pragma once

#include <allio/error.hpp>

#include <vsm/lazy.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/utility.hpp>

#include <memory>
#include <new>

namespace allio {
namespace detail {

struct operator_deleter
{
	void vsm_static_operator_invoke(void* const storage)
	{
		operator delete(storage);
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
