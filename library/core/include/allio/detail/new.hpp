#pragma once

#include <allio/detail/new.h>

#include <vsm/concepts.hpp>
#include <vsm/platform.h>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/utility.hpp>

#include <memory>

namespace allio::detail {

using allocation = allio_allocation;

[[nodiscard]] vsm_always_inline inline allocation acquire_storage(
	size_t const min_size,
	size_t const max_size,
	size_t const alignment,
	allio_allocation_strategy const strategy)
{
	return allio_acquire_storage(min_size, max_size, alignment, strategy);
}

vsm_always_inline inline void release_storage(
	void* const storage,
	size_t const size_hint,
	size_t const alignment,
	allio_allocation_strategy const strategy)
{
	return allio_release_storage(storage, size_hint, alignment, strategy);
}


template<typename T>
inline constexpr size_t new_size_for = sizeof(T);

template<>
inline constexpr size_t new_size_for<void> = 1;

template<typename T>
inline constexpr size_t new_alignment_for = alignof(T);

template<>
inline constexpr size_t new_alignment_for<void> = alignof(std::max_align_t);


class storage_deleter
{
	size_t m_size;

public:
	explicit storage_deleter(size_t const size)
		: m_size(size)
	{
	}

	template<typename T>
	void operator()(T* const storage) const
	{
		detail::release_storage(
			static_cast<void*>(storage),
			m_size,
			new_alignment_for<T>,
			allio_allocation_strategy_generic);
	}
};

template<vsm::non_cvref T>
using unique_storage_ptr = std::unique_ptr<T, storage_deleter>;

[[nodiscard]] vsm::result<unique_storage_ptr<void>> _allocate_unique(
	size_t size,
	size_t alignment,
	size_t element_size);

template<vsm::non_cvref T = void>
	requires std::is_void_v<T> || (std::is_object_v<T> && !std::is_array_v<T>)
[[nodiscard]] vsm::result<unique_storage_ptr<T>> allocate_unique(size_t const size)
{
	auto r = _allocate_unique(size, new_alignment_for<T>, new_size_for<T>);

	if constexpr (std::is_void_v<T>)
	{
		return r;
	}
	else if (r)
	{
		return vsm::result<unique_storage_ptr<T>>(
			vsm::result_value,
			static_cast<T*>(r->release()),
			r->get_deleter());
	}
	else
	{
		return vsm::unexpected(r.error());
	}
}


template<vsm::non_cvref T>
void delete_object(T* const object)
{
	vsm_assert(object != nullptr); //PRECONDITION

	object->~T();

	detail::release_storage(
		object,
		sizeof(T),
		alignof(T),
		allio_allocation_strategy_generic);
}

struct object_deleter
{
	template<vsm::non_cvref T>
	vsm_static_operator void operator()(T* const object) vsm_static_operator_const
	{
		detail::delete_object(object);
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

} // namespace allio::detail
