#pragma once

#include <vsm/utility.hpp>

#include <initializer_list>
#include <memory>
#include <ranges>

namespace allio {

//TODO: Use std::inplace_vector

template<typename T, size_t Capacity>
struct inplace_vector_storage
{
	size_t m_size = 0;
	union
	{
		unsigned char m_dummy;
		T m_data[Capacity];
	};

	constexpr inplace_vector_storage() noexcept
		: m_size(0)
	{
	}

	constexpr ~inplace_vector_storage() noexcept
	{
		std::destroy_n(m_data, m_size);
	}

	void set_size(size_t const size)
	{
		m_size = size;
	}
};

template<typename T>
struct inplace_vector_storage<T, 0>
{
	static constexpr size_t m_size = 0;
	static constexpr T* m_data = nullptr;

	void set_size(size_t)
	{
	}
};

template<typename T, size_t Capacity>
class inplace_vector : inplace_vector_storage<T, Capacity>
{
public:
	inplace_vector() = default;

	[[nodiscard]] size_t size() const
	{
		return this->m_size;
	}

	[[nodiscard]] size_t capacity() const
	{
		return Capacity;
	}

	[[nodiscard]] T& front()
	{
		vsm_assert(this->m_size != 0);
		return this->m_data[0];
	}

	[[nodiscard]] T const& front() const
	{
		vsm_assert(this->m_size != 0);
		return this->m_data[0];
	}

	[[nodiscard]] T& back()
	{
		vsm_assert(this->m_size != 0);
		return this->m_data[this->m_size - 1];
	}

	[[nodiscard]] T const& back() const
	{
		vsm_assert(this->m_size != 0);
		return this->m_data[this->m_size - 1];
	}

	[[nodiscard]] T* data()
	{
		return this->m_data;
	}

	[[nodiscard]] T const* data() const
	{
		return this->m_data;
	}

	[[nodiscard]] T& operator[](size_t const index)
	{
		vsm_assert(index < this->m_size);
		return this->m_data[index];
	}

	[[nodiscard]] T const& operator[](size_t const index) const
	{
		vsm_assert(index < this->m_size);
		return this->m_data[index];
	}

	template<std::convertible_to<T> U = T>
	T& unchecked_push_back(U&& value)
	{
		return unchecked_emplace_back(vsm_forward(value));
	}

	template<typename... Args>
	T& unchecked_emplace_back(Args&&... args)
	{
		vsm_assert(this->m_size < Capacity);
		T* const ptr = std::construct_at(this->m_data + this->m_size, vsm_forward(args)...);
		this->set_size(this->m_size + 1);
		return *ptr;
	}

	template<std::convertible_to<T> U = T>
	[[nodiscard]] T* try_push_back(U&& value)
	{
		return try_emplace_back(vsm_forward(value));
	}

	template<typename... Args>
		requires std::constructible_from<T, Args...>
	[[nodiscard]] T* try_emplace_back(Args&&... args)
	{
		if (this->m_size == Capacity)
		{
			return nullptr;
		}

		T* const ptr = std::construct_at(this->m_data + this->m_size, vsm_forward(args)...);
		this->set_size(this->m_size + 1);
		return ptr;
	}

	template<std::ranges::range R>
	void append_range(R&& range);

	void clear()
	{
		std::destroy_n(this->m_data, this->m_size);
		this->set_size(0);
	}

	[[nodiscard]] T* begin()
	{
		return this->m_data;
	}

	[[nodiscard]] T const* begin() const
	{
		return this->m_data;
	}

	[[nodiscard]] T* end()
	{
		return this->m_data + this->m_size;
	}

	[[nodiscard]] T const* end() const
	{
		return this->m_data + this->m_size;
	}
};

} // namespace allio
