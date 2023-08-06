#pragma once

namespace allio {

//TODO: Use std::inplace_vector

template<typename T, size_t MaxSize>
class bounded_vector
{
	size_t m_size = 0;
	alignas(T) std::byte m_storage[MaxSize * sizeof(T)];

public:
	size_t size() const
	{
		return m_size;
	}

	T* data()
	{
		return reinterpret_cast<T*>(m_storage);
	}

	T const* data() const
	{
		return reinterpret_cast<T const*>(m_storage);
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
};

} // namespace allio
