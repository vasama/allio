#pragma once

#include <allio/detail/mutable_buffer.hpp>
#include <allio/detail/new.h>

namespace allio::detail {

template<typename T, size_t LocalSize, size_t Alignment = alignof(T)>
class default_sequence_container
{
	static_assert(std::is_trivially_default_constructible_v<T>);
	static_assert(std::is_trivially_copyable_v<T>);

	size_t m_size = LocalSize;
	union
	{
		alignas(Alignment) T m_data[LocalSize];
		void* m_data_ptr;
	};

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

	default_sequence_container() = default;

	default_sequence_container(default_sequence_container&& other)
		: m_size(other.m_size)
	{
		if (other.m_size > LocalSize)
		{
			m_data_ptr = other.m_data_ptr;
			::new (other.m_data) T[LocalSize];
			other.m_size = LocalSize;
		}
		else
		{
			std::memcpy(m_data, other.m_data, sizeof(m_data));
		}
	}

	default_sequence_container& operator=(default_sequence_container&& other)
	{
		if (m_size > LocalSize)
		{
			::allio_release_storage(
				m_data_ptr,
				m_size * sizeof(T),
				Alignment,
				allio_allocation_strategy_generic);

			::new (m_data) T[LocalSize];
			m_size = LocalSize;
		}

		m_size = other.m_size;

		if (other.m_size > LocalSize)
		{
			m_data_ptr = other.m_data_ptr;
			::new (other.m_data) T[LocalSize];
			other.m_size = LocalSize;
		}
		else
		{
			std::memcpy(m_data, other.m_data, sizeof(m_data));
		}

		return *this;
	}

	~default_sequence_container()
	{
		if (m_size > LocalSize)
		{
			::allio_release_storage(
				m_data_ptr,
				m_size * sizeof(T),
				Alignment,
				allio_allocation_strategy_generic);
		}
	}


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
		return m_size > LocalSize ? static_cast<T*>(m_data_ptr) : m_data;
	}

	[[nodiscard]] T const* data() const
	{
		return m_size > LocalSize ? static_cast<T*>(m_data_ptr) : m_data;
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

	[[nodiscard]] vsm::result<std::span<T>> resize(size_t const size)
	{
		return resize(size, size);
	}

	[[nodiscard]] vsm::result<std::span<T>> resize(size_t const min_size, size_t const max_size)
	{
		if (min_size > m_size)
		{
			auto const allocation = ::allio_acquire_storage(
				min_size * sizeof(T),
				max_size * sizeof(T),
				Alignment,
				allio_allocation_strategy_generic);

			if (allocation.storage == nullptr)
			{
				return vsm::unexpected(error::not_enough_memory);
			}

			if (m_size > LocalSize)
			{
				::allio_release_storage(
					m_data_ptr,
					m_size * sizeof(T),
					Alignment,
					allio_allocation_strategy_generic);
			}

			m_data_ptr = allocation.storage;
			m_size = allocation.size / sizeof(T);
		}

		return std::span<T>(data(), m_size);
	}

private:
	[[nodiscard]] friend vsm::result<size_t> tag_invoke(
		resize_container_t,
		default_sequence_container& self,
		size_t const min_size,
		size_t const max_size)
	{
		vsm_try(storage, self.resize(min_size, max_size));
		return storage.size();
	}
};

} // namespace allio::detail
