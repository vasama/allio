#pragma once

#include <vsm/utility.hpp>

#include <initializer_list>
#include <memory>

namespace allio {

//TODO: Use std::inplace_vector

template<typename T, size_t MaxSize>
class bounded_vector
{
	size_t m_size = 0;
	alignas(T) std::byte m_storage[MaxSize * sizeof(T)];

public:
	bounded_vector() = default;

	bounded_vector(std::initializer_list<T> const list)
	{
		std::uninitialized_copy_n(
			list.data(),
			list.size(),
			reinterpret_cast<T*>(m_storage));
	}

	size_t size() const
	{
		return m_size;
	}

	T& front()
	{
		return reinterpret_cast<T*>(m_storage)[0];
	}

	T const& front() const
	{
		return reinterpret_cast<T const*>(m_storage)[0];
	}

	T* data()
	{
		return reinterpret_cast<T*>(m_storage);
	}

	T const* data() const
	{
		return reinterpret_cast<T const*>(m_storage);
	}

	T& operator[](size_t const index)
	{
		vsm_assert(index < MaxSize);
		return reinterpret_cast<T*>(m_storage)[index];
	}

	T const& operator[](size_t const index) const
	{
		vsm_assert(index < MaxSize);
		return reinterpret_cast<T const*>(m_storage)[index];
	}

	template<std::convertible_to<T> U = T>
	void push_back(U&& value)
	{
		vsm_assert(m_size < MaxSize);
		T* const ptr = new (reinterpret_cast<T*>(m_storage) + m_size) T(vsm_forward(value));
		++m_size;
	}

	template<std::convertible_to<T> U = T>
	T* try_push_back(U&& value)
	{
		if (m_size < MaxSize)
		{
			T* const ptr = new (reinterpret_cast<T*>(m_storage) + m_size) T(vsm_forward(value));
			++m_size;
			return ptr;
		}
		return nullptr;
	}

	template<typename R>
	void append_range(R&& range)
	{
	}

	T* begin()
	{
		return reinterpret_cast<T*>(m_storage);
	}

	T const* begin() const
	{
		return reinterpret_cast<T const*>(m_storage);
	}

	T* end()
	{
		return reinterpret_cast<T*>(m_storage) + m_size;
	}

	T const* end() const
	{
		return reinterpret_cast<T const*>(m_storage) + m_size;
	}
};

} // namespace allio
