#pragma once

#include <allio/detail/io.hpp>
#include <allio/detail/mutable_buffer.hpp>
#include <allio/detail/new.hpp>
#include <allio/impl/error_encoding.hpp>

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
	io_extension_base(initialize_extension extension)
		: m_extension(&extension.extension)
	{
		m_extension->extension = nullptr;
	}

	io_extension_base(acquire_extension extension)
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
		alignas(std::max_align_t) unsigned char data[];

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
			m_extension->extension = nullptr;

			while (block_t* const block = std::exchange(head, head->next))
			{
				release_storage(
					block,
					block->size,
					alignof(std::max_align_t),
					allio_allocation_strategy_generic);
			}
		}
	}


	[[nodiscard]] vsm::allocation allocate(
		size_t const min_size,
		size_t const max_size,
		size_t const alignment = alignof(std::max_align_t)) const
	{
		vsm_assert(m_extension != nullptr); //PRECONDITION

		auto const allocation = acquire_storage(
			offsetof(block_t, data) + min_size,
			offsetof(block_t, data) + max_size,
			alignof(std::max_align_t),
			allio_allocation_strategy_generic);

		if (allocation.storage == nullptr)
		{
			return { nullptr };
		}

		block_t* const block = ::new (allocation.storage) block_t(allocation.size);
		block->next = static_cast<block_t*>(m_extension->extension);
		m_extension->extension = block;

		return { allocation.storage, allocation.size };
	}

	void deallocate(vsm::allocation) const
	{
	}

private:
	friend vsm::result<vsm::allocation> tag_invoke(
		get_storage_t,
		io_extension_allocator& list,
		size_t const min_size,
		size_t const max_size,
		std::align_val_t const min_alignment)
	{
		return list.allocate(min_size, max_size, static_cast<size_t>(min_alignment));
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
};

} // namespace allio::detail
