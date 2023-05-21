#pragma once

#include <bit>
#include <type_traits>

#include <cstdint>

namespace allio {

class untyped_parameter
{
	template<typename T>
	static constexpr bool is_direct =
		sizeof(T) <= sizeof(uintptr_t) &&
		alignof(T) <= alignof(uintptr_t) &&
		std::is_trivially_copyable_v<T>;

	std::byte m_storage alignas(uintptr_t)[sizeof(uintptr_t)];

public:
	untyped_parameter() = default;

	template<typename T>
	static untyped_parameter make(std::type_identity_t<T> const& value)
	{
		return untyped_parameter(value);
	}

	template<typename T>
	T const& get() const
	{
		if constexpr (is_direct<T>)
		{
			return reinterpret_cast<T const&>(m_storage);
		}
		else
		{
			return *reinterpret_cast<T const* const&>(m_storage);
		}
	}

	template<typename T>
	explicit operator T const&() const
	{
		return get<T>();
	}

private:
	template<typename T>
	explicit untyped_parameter(T const& value)
	{
		if constexpr (is_direct<T>)
		{
			new (m_storage) T(value);
		}
		else
		{
			new (m_storage) T const*(&value);
		}
	}
};

} // namespace allio
