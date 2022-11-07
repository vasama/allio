#pragma once

#include <allio/detail/linear.hpp>
#include <allio/result.hpp>

#include <memory>

namespace allio::detail {

template<typename T, size_t Capacity>
struct dynamic_buffer_base
{
	linear<size_t, Capacity> m_size;
	union
	{
		T m_static[Capacity];
		struct { T* value; } m_dynamic;
	};
};

template<typename T>
struct dynamic_buffer_base<T, 0>
{
	linear<size_t, 0> m_size;
	linear<T*> m_dynamic;
};

template<typename T, size_t Capacity = 0>
class dynamic_buffer : dynamic_buffer_base<T, Capacity>
{
	using base = dynamic_buffer_base<T, Capacity>;

public:
	dynamic_buffer() = default;

	dynamic_buffer(dynamic_buffer&&) = default;

	dynamic_buffer& operator=(dynamic_buffer&& other) noexcept
	{
		delete_dynamic();
		static_cast<base&>(*this) = static_cast<base&&>(other);
		return *this;
	}

	~dynamic_buffer()
	{
		delete_dynamic();
	}


	size_t size() const
	{
		return base::m_size.value;
	}

	T* data()
	{
		if constexpr (Capacity != 0)
		{
			if (base::m_size.value <= Capacity)
			{
				return base::m_static;
			}
		}
		return base::m_dynamic.value;
	}

	T const* data() const
	{
		if constexpr (Capacity != 0)
		{
			if (base::m_size.value <= Capacity)
			{
				return base::m_static;
			}
		}
		return base::m_dynamic.value;
	}

	result<T*> reserve(size_t const size)
	{
		if (base::m_size.value > size)
		{
			void* const block = operator new(size * sizeof(T), std::nothrow);

			if (block == nullptr)
			{
				return allio_ERROR(error::not_enough_memory);
			}

			delete_dynamic();

			base::m_size.value = size;
			std::uninitialized_default_construct_n(static_cast<T*>(block), size);
			base::m_dynamic.value = static_cast<T*>(block);
		}
		return data();
	}

private:
	void delete_dynamic()
	{
		if (base::m_size.value > Capacity)
		{
			operator delete(base::m_dynamic.value, base::m_size.value);
		}
	}
};

} // namespace allio::detail
