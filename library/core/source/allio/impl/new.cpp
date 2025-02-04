#include <allio/detail/new.hpp>

#include <allio/error.hpp>
#include <allio/impl/error_encoding.hpp>

#include <new>

using namespace allio;
using namespace allio::detail;

extern "C"
allio_allocation allio_acquire_storage(
	size_t const min_size,
	size_t /* max_size */,
	size_t const alignment,
	allio_allocation_strategy)
{
	void* const storage = operator new(
		min_size,
		static_cast<std::align_val_t>(alignment),
		std::nothrow);

	return
	{
		storage,
		storage != nullptr
			? min_size
			: static_cast<size_t>(0),
	};
}

extern "C"
void allio_release_storage(
	void* const storage,
	size_t /* size_hint */,
	size_t const alignment,
	allio_allocation_strategy)
{
	operator delete(storage, static_cast<std::align_val_t>(alignment), std::nothrow);
}

[[nodiscard]] vsm::result<unique_storage_ptr<void>> detail::_allocate_unique(
	size_t size,
	size_t const alignment,
	size_t const element_size)
{
	if (element_size != 1)
	{
		if (size > std::numeric_limits<size_t>::max() / element_size)
		{
			return vsm::unexpected(allio_error(error::not_enough_memory));
		}

		size = size * element_size;
	}

	auto const allocation = detail::acquire_storage(
		/* min_size: */ size,
		/* max_size: */ static_cast<size_t>(-1),
		alignment,
		allio_allocation_strategy_generic);

	if (allocation.storage == nullptr)
	{
		return vsm::unexpected(allio_error(error::not_enough_memory));
	}

	return vsm::result<unique_storage_ptr<void>>(
		vsm::result_value,
		allocation.storage,
		storage_deleter(allocation.size));
}
