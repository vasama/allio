#pragma once

#include <allio/detail/io.hpp>
#include <allio/detail/new.hpp>
#include <allio/impl/error_encoding.hpp>

#include <vsm/allocator.hpp>

namespace allio::detail {

struct initialize_extension
{
	async_extension& extension;

	explicit initialize_extension(async_extension& extension)
		: extension(extension)
	{
	}
};

struct acquire_extension
{
	async_extension& extension;

	explicit acquire_extension(async_extension& extension)
		: extension(extension)
	{
	}
};

class io_extension_base
{
protected:
	async_extension* m_extension;

public:
	io_extension_base(initialize_extension const extension)
		: m_extension(&extension.extension)
	{
		m_extension->extension = nullptr;
	}

	io_extension_base(acquire_extension const extension)
		: m_extension(&extension.extension)
	{
	}

	io_extension_base(io_extension_base const&) = delete;
	io_extension_base& operator=(io_extension_base const&) = delete;

	void release()
	{
		if (m_extension != nullptr)
		{
			m_extension->extension = nullptr;
		}
	}

protected:
	~io_extension_base() = default;
};


class io_extension_allocator : public io_extension_base
{
	struct block_t
	{
		block_t* next;
		size_t size;

		alignas(std::max_align_t) unsigned char storage[];

		explicit block_t(size_t const size)
			: next(nullptr)
			, size(size)
		{
		}
	};

public:
	using io_extension_base::io_extension_base;

	~io_extension_allocator()
	{
		if (m_extension != nullptr && m_extension->extension != nullptr)
		{
			block_t* head = static_cast<block_t*>(m_extension->extension);

			do
			{
				block_t* const block = std::exchange(head, head->next);

				allio_release_storage(
					block,
					block->size,
					alignof(block_t),
					allio_allocation_strategy_generic);
			}
			while (head != nullptr);
		}
	}


	[[nodiscard]] vsm::result<vsm::allocation> get_storage(
		size_t const min_size,
		size_t const max_size,
		std::align_val_t const min_alignment) const
	{
		static constexpr size_t storage_offset = offsetof(block_t, storage);

		vsm_assert(m_extension != nullptr); //PRECONDITION

		if (static_cast<size_t>(min_alignment) > alignof(std::max_align_t))
		{
			return vsm::unexpected(allio_error(error::insufficient_alignment));
		}

		auto const allocation = allio_acquire_storage(
			storage_offset + min_size,
			storage_offset + max_size,
			alignof(block_t),
			allio_allocation_strategy_generic);

		if (allocation.storage == nullptr)
		{
			return vsm::unexpected(allio_error(error::not_enough_memory));
		}

		block_t* const block = ::new (allocation.storage) block_t(allocation.size);
		block->next = static_cast<block_t*>(m_extension->extension);
		m_extension->extension = block;

		return vsm::allocation(block->storage, allocation.size - storage_offset);
	}
};

template<typename T>
class io_extension_object : public io_extension_base
{
public:
	using io_extension_base::io_extension_base;

	~io_extension_object()
	{
		if (m_extension != nullptr && m_extension->extension != nullptr)
		{
			delete_object(static_cast<T*>(m_extension->extension));
		}
	}


	[[nodiscard]] bool has_object() const
	{
		vsm_assert(m_extension != nullptr); //PRECONDITION
		return m_extension->extension != nullptr;
	}

	[[nodiscard]] vsm::result<T*> emplace_default()
	{
		vsm_assert(m_extension != nullptr); //PRECONDITION
		vsm_assert(m_extension->extension == nullptr); //PRECONDITION

		vsm_try(ptr, detail::allocate_unique<T>());
		m_extension->extension = ::new (ptr.release()) T;

		return static_cast<T*>(m_extension->extension);
	}

	[[nodiscard]] vsm::result<T*> emplace(auto&&... args)
	{
		vsm_assert(m_extension != nullptr); //PRECONDITION
		vsm_assert(m_extension->extension == nullptr); //PRECONDITION

		vsm_try(ptr, detail::make_unique<T>(vsm_forward(args)...));
		m_extension->extension = ptr.release();

		return static_cast<T*>(m_extension->extension);
	}

	[[nodiscard]] T* get() const
	{
		vsm_assert(m_extension != nullptr); //PRECONDITION
		vsm_assert(m_extension->extension != nullptr); //PRECONDITION
		return static_cast<T*>(m_extension->extension);
	}

	[[nodiscard]] T* operator->() const
	{
		return get();
	}
};

} // namespace allio::detail
