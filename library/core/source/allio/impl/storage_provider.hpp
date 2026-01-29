#pragma once

#include <allio/detail/new.h>
#include <allio/impl/io_extension.hpp>

#include <vsm/concepts.hpp>

namespace allio {

template<typename StorageProvider>
concept storage_provider = requires (StorageProvider& storage_provider)
{
	storage_provider.get_storage(size_t(0), size_t(0), std::align_val_t(0));
};

template<typename T>
class inplace_storage_provider
{
	alignas(T) unsigned char m_storage[sizeof(T)];

public:
	inplace_storage_provider() = default;
	inplace_storage_provider(inplace_storage_provider const&) = delete;
	inplace_storage_provider& operator=(inplace_storage_provider const&) = delete;

	[[nodiscard]] vsm::result<vsm::allocation> get_storage(
		size_t const min_size,
		std::align_val_t const min_alignment) &
	{
		return get_storage(min_size, min_size, min_alignment);
	}

	[[nodiscard]] vsm::result<vsm::allocation> get_storage(
		size_t const min_size,
		size_t const max_size,
		std::align_val_t const min_alignment) &
	{
		if (min_size > sizeof(m_storage))
		{
			return vsm::unexpected(allio_error(error::no_buffer_space));
		}

		if (static_cast<size_t>(min_alignment) > alignof(T))
		{
			return vsm::unexpected(allio_error(error::insufficient_alignment));
		}

		return vsm::allocation(m_storage, sizeof(m_storage));
	}
};

template<size_t Size>
class dynamic_storage_provider
{
	size_t m_size = Size;

	union
	{
		alignas(std::max_align_t) unsigned char m_storage[Size];
		void* m_storage_ptr;
	};

public:
	dynamic_storage_provider() = default;
	dynamic_storage_provider(dynamic_storage_provider const&) = delete;
	dynamic_storage_provider& operator=(dynamic_storage_provider const&) = delete;

	[[nodiscard]] vsm::result<vsm::allocation> get_storage(
		size_t const min_size,
		std::align_val_t const min_alignment) &
	{
		return get_storage(min_size, min_size, min_alignment);
	}

	[[nodiscard]] vsm::result<vsm::allocation> get_storage(
		size_t const min_size,
		size_t const max_size,
		std::align_val_t const min_alignment) &
	{
		if (static_cast<size_t>(min_alignment) < alignof(std::max_align_t))
		{
			return vsm::unexpected(allio_error(error::insufficient_alignment));
		}

		if (m_size >= min_size)
		{
			void* const storage = m_size <= Size
				? m_storage
				: m_storage_ptr;

			return vsm::allocation(storage, m_size);
		}

		auto const allocation = allio_acquire_storage(
			min_size,
			max_size,
			alignof(std::max_align_t),
			allio_allocation_strategy_generic);

		if (allocation.storage == nullptr)
		{
			return vsm::unexpected(allio_error(error::not_enough_memory));
		}

		if (m_size > Size)
		{
			allio_release_storage(
				m_storage_ptr,
				m_size,
				alignof(std::max_align_t),
				allio_allocation_strategy_generic);
		}

		m_storage_ptr = allocation.storage;
		m_size = allocation.size;

		return vsm::allocation(allocation.storage, allocation.size);
	}
};

class storage_provider_ref
{
	using get_storage_t = vsm::result<vsm::allocation>(
		void const* context,
		size_t min_size,
		size_t max_size,
		std::align_val_t min_alignment);

	get_storage_t* m_function;
	void const* m_context;

public:
	template<storage_provider StorageProvider>
		requires vsm::no_cvref_of<StorageProvider, storage_provider_ref>
	storage_provider_ref(StorageProvider& storage_provider)
		: m_function(_get_storage<StorageProvider>)
		, m_context(std::addressof(storage_provider))
	{
	}

	[[nodiscard]] vsm::result<vsm::allocation> get_storage(
		size_t const min_size,
		std::align_val_t const min_alignment) const
	{
		return m_function(m_context, min_size, min_size, min_alignment);
	}

	[[nodiscard]] vsm::result<vsm::allocation> get_storage(
		size_t const min_size,
		size_t const max_size,
		std::align_val_t const min_alignment) const
	{
		return m_function(m_context, min_size, max_size, min_alignment);
	}

private:
	template<typename StorageProvider>
	static vsm::result<vsm::allocation> _get_storage(
		void const* const context,
		size_t const min_size,
		size_t const max_size,
		std::align_val_t const min_alignment)
	{
		auto& storage_provider = *const_cast<StorageProvider*>(
			static_cast<StorageProvider const*>(context));

		return storage_provider.get_storage(min_size, max_size, min_alignment);
	}
};

} // namespace allio
