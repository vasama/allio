#pragma once

#include <allio/detail/new.hpp>
#include <allio/error.hpp>

#include <vsm/concepts.hpp>
#include <vsm/lazy.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/utility.hpp>

#include <memory>

namespace allio {

template<typename T>
inline constexpr size_t alignment_for = alignof(T);

template<>
inline constexpr size_t alignment_for<void> = alignof(std::max_align_t);


class storage_deleter
{
	size_t m_size;

public:
	explicit storage_deleter(size_t const size)
		: m_size(size)
	{
	}

	template<typename T>
	vsm_static_operator void operator()(T* const storage) vsm_static_operator_const
	{
		detail::release_storage(
			static_cast<void*>(storage),
			m_size,
			alignment_for<T>,
			/* automatic: */ false);
	}
};

template<vsm::non_cvref T = void>
using unique_storage_ptr = std::unique_ptr<T, storage_deleter>;

template<vsm::non_cvref T = void>
[[nodiscard]] vsm::result<unique_storage_ptr<T>> allocate_unique(size_t size)
{
	static_assert(std::is_object_v<T> || std::is_void_v<T>);
	static_assert(!std::is_array_v<T>);

	if constexpr (std::is_object_v<T>)
	{
		size = size * sizeof(T);
	}

	auto const allocation = detail::acquire_storage(
		/* min_size: */ size,
		/* max_size: */ static_cast<size_t>(-1),
		/* alignment: */ alignment_for<T>,
		/* automatic: */ false);

	if (allocation.storage == nullptr)
	{
		return vsm::unexpected(error::not_enough_memory);
	}

	return vsm::result<unique_storage_ptr<T>>(
		vsm::result_value,
		reinterpret_cast<T*>(allocation.storage),
		storage_deleter(allocation.size));
}


struct object_deleter
{
	template<vsm::non_cvref T>
	vsm_static_operator void operator()(T* const object) vsm_static_operator_const
	{
		object->~T();

		detail::release_storage(
			object,
			sizeof(T),
			alignof(T),
			/* automatic: */ false);
	}
};

template<typename T>
using unique_ptr = std::unique_ptr<T, object_deleter>;

template<vsm::non_cvref T>
[[nodiscard]] vsm::result<unique_ptr<T>> make_unique(auto&&... args)
{
	vsm_try(storage, allocate_unique<T>(1));

	T* const object = ::new (storage.get()) T(vsm_forward(args)...);
	storage.release();

	return vsm::result<unique_ptr<T>>(vsm::result_value, object);
}


#if 0
namespace detail {

struct operator_deleter
{
	vsm_static_operator void operator()(void* const storage) vsm_static_operator_const
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
#endif

} // namespace allio
