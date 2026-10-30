#pragma once

#include <allio/detail/mutable_buffer.hpp>
#include <allio/detail/new.h>

#include <span>

namespace allio::detail {

enum class resize_mode
{
	discard,
	append,
	prepend,
};

template<typename T, size_t Size, size_t Alignment = alignof(T)>
class basic_storage;

template<typename T, size_t Alignment = alignof(T)>
class basic_storage_base
{
	static_assert(std::is_trivially_default_constructible_v<T>);
	static_assert(std::is_trivially_copyable_v<T>);

	size_t m_size : sizeof(size_t) * CHAR_BIT - 1;
	size_t m_is_non_local : 1;

public:
	using value_type                    = T;
	using size_type                     = size_t;
	using difference_type               = ptrdiff_t;
	using reference                     = value_type&;
	using const_reference               = value_type const&;
	using pointer                       = value_type*;
	using const_pointer                 = value_type const*;
	using iterator                      = value_type*;
	using const_iterator                = value_type const*;


	[[nodiscard]] bool empty() const
	{
		return false;
	}

	[[nodiscard]] size_t size() const
	{
		return m_size;
	}

	[[nodiscard]] T* data()
	{
		return m_is_non_local ? get_storage_ptr() : get_storage();
	}

	[[nodiscard]] T const* data() const
	{
		return m_is_non_local ? get_storage_ptr() : get_storage();
	}

	[[nodiscard]] iterator begin()
	{
		return data();
	}

	[[nodiscard]] const_iterator begin() const
	{
		return data();
	}

	[[nodiscard]] iterator end()
	{
		return data() + m_size;
	}

	[[nodiscard]] const_iterator end() const
	{
		return data() + m_size;
	}

	[[nodiscard]] vsm::result<std::span<T>> resize(
		size_t const size,
		resize_mode const mode = resize_mode::append)
	{
		return resize(size, size, mode);
	}

	[[nodiscard]] vsm::result<std::span<T>> resize(
		size_t const min_size,
		size_t const max_size,
		resize_mode const mode = resize_mode::append)
	{
		size_t const size = m_size;

		if (size >= min_size)
		{
			return std::span<T>(data(), size);
		}

		auto const allocation = ::allio_acquire_storage(
			min_size * sizeof(T),
			max_size * sizeof(T),
			Alignment,
			allio_allocation_strategy_generic);

		if (allocation.storage == nullptr)
		{
			return vsm::unexpected(error::not_enough_memory);
		}

		T* const new_data = static_cast<T*>(allocation.storage);
		size_t const new_size = allocation.size / sizeof(T);
		{
			T* new_beg = new_data;
			T* new_end = new_data + new_size;

			switch (mode)
			{
			case resize_mode::discard:
				break;

			case resize_mode::append:
				std::uninitialized_copy_n(data(), size, new_beg);
				new_beg += size;
				break;

			case resize_mode::prepend:
				std::uninitialized_copy_n(data(), size, new_end - size);
				new_end -= size;
				break;
			}

			std::uninitialized_default_construct(new_beg, new_end);
		}

		if (m_is_non_local)
		{
			::allio_release_storage(
				get_storage_ptr(),
				size * sizeof(T),
				Alignment,
				allio_allocation_strategy_generic);
		}

		set_storage_ptr(new_data);
		set_size(new_size, true);

		return std::span<T>(new_data, new_size);
	}

protected:
	basic_storage_base() = default;
	basic_storage_base(basic_storage_base const&) = default;
	basic_storage_base& operator=(basic_storage_base const&) = default;
	~basic_storage_base() = default;

private:
	void set_size(size_t const size, bool const is_non_local)
	{
		vsm_gcc_diagnostic(push)
		vsm_gcc_diagnostic(ignored "-Wconversion")

		m_size = size;
		m_is_non_local = is_non_local;

		vsm_gcc_diagnostic(pop)
	}

	[[nodiscard]] T* get_storage() const
	{
		return const_cast<T*>(reinterpret_cast<T const*>(this + 1));
	}

	[[nodiscard]] T* get_storage_ptr() const
	{
		return *reinterpret_cast<T* const*>(this + 1);
	}

	void set_storage_ptr(T* const ptr)
	{
		*reinterpret_cast<T**>(this + 1) = ptr;
	}

	[[nodiscard]] friend vsm::result<size_t> tag_invoke(
		resize_container_t,
		basic_storage_base& self,
		size_t const min_size,
		size_t const max_size) noexcept
	{
		vsm_try(storage, self.resize(min_size, max_size));
		return storage.size();
	}

	template<typename, size_t, size_t>
	friend class basic_storage;
};

template<typename T, size_t Size, size_t Alignment>
class basic_storage_storage : public basic_storage_base<T, Alignment>
{
	union
	{
		alignas(Alignment) T m_storage[Size];
		T* m_storage_ptr;
	};

	friend basic_storage<T, Size, Alignment>;
};

template<typename T, size_t Size, size_t Alignment>
class basic_storage : public basic_storage_storage<T, Size, Alignment>
{
	using base_type = basic_storage_base<T, Alignment>;
	using storage_type = basic_storage_storage<T, Size, Alignment>;
	static_assert(offsetof(storage_type, m_storage) == sizeof(base_type));

public:
	basic_storage()
	{
		::new (this->m_storage) T[Size];
		this->set_size(Size, false);
	}

	basic_storage(basic_storage&& other)
	{
		this->set_size(other.m_size, other.m_is_non_local);

		if (other.m_is_non_local)
		{
			this->m_storage_ptr = other.m_storage_ptr;
			::new (other.m_storage) T[Size];

			other.set_size(Size, false);
		}
		else
		{
			std::memcpy(this->m_storage, other.m_storage, sizeof(storage_type::m_storage));
		}
	}

	basic_storage& operator=(basic_storage&& other)
	{
		if (this != &other)
		{
			if (this->m_is_non_local)
			{
				::allio_release_storage(
					this->m_storage_ptr,
					this->m_size * sizeof(T),
					Alignment,
					allio_allocation_strategy_generic);

				::new (this->m_storage) T[Size];
			}

			this->set_size(other.m_size, other.m_is_non_local);

			if (other.m_is_non_local)
			{
				this->m_storage_ptr = other.m_storage_ptr;
				::new (other.m_storage) T[Size];
				this->set_size(Size, false);
			}
			else
			{
				std::memcpy(this->m_storage, other.m_storage, sizeof(storage_type::m_storage));
			}
		}

		return *this;
	}

	~basic_storage()
	{
		if (this->m_is_non_local)
		{
			::allio_release_storage(
				this->m_storage_ptr,
				this->m_size * sizeof(T),
				Alignment,
				allio_allocation_strategy_generic);
		}
	}
};


using untyped_storage_base = basic_storage_base<std::byte, alignof(std::max_align_t)>;

template<size_t Size>
using untyped_storage = basic_storage<std::byte, Size, alignof(std::max_align_t)>;

} // namespace allio::detail
