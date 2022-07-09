#pragma once

#include <compare>

namespace allio {

namespace detail {

struct type_id_base;

struct type_id_object {};

template<typename T>
inline detail::type_id_object const type_id_instance = {};

} // namespace detail


template<typename Base = detail::type_id_base>
class type_id;

template<>
class type_id<detail::type_id_base>
{
	detail::type_id_object const* m_pointer;

public:
	template<typename T>
	constexpr type_id(type_id<T> const& other)
		: m_pointer(other.m_pointer)
	{
	}

private:
	friend bool operator==(type_id const& lhs, type_id const& rhs)
	{
		return lhs.m_pointer == rhs.m_pointer;
	}

	friend bool operator!=(type_id const& lhs, type_id const& rhs)
	{
		return lhs.m_pointer != rhs.m_pointer;
	}

	friend std::strong_ordering operator<=>(type_id const& lhs, type_id const& rhs)
	{
		return std::compare_three_way()(lhs.m_pointer, rhs.m_pointer);
	}
};

template<typename Base>
class type_id
{
	detail::type_id_object const* m_pointer;

public:
	constexpr type_id(detail::type_id_object const* pointer)
		: m_pointer(pointer)
	{
	}

	template<std::derived_from<Base> Derived>
	constexpr type_id(type_id<Derived> const& derived)
		: m_pointer(derived.m_pointer)
	{
	}

private:
	friend bool operator==(type_id const& lhs, type_id const& rhs)
	{
		return lhs.m_pointer == rhs.m_pointer;
	}

	template<std::derived_from<Base> Rhs>
	friend bool operator==(type_id const& lhs, type_id<Rhs> const& rhs)
	{
		return lhs.m_pointer == rhs.m_pointer;
	}

	friend bool operator!=(type_id const& lhs, type_id const& rhs)
	{
		return lhs.m_pointer != rhs.m_pointer;
	}

	template<std::derived_from<Base> Rhs>
	friend bool operator!=(type_id const& lhs, type_id<Rhs> const& rhs)
	{
		return lhs.m_pointer != rhs.m_pointer;
	}

	friend std::strong_ordering operator<=>(type_id const& lhs, type_id const& rhs)
	{
		return std::compare_three_way()(lhs.m_pointer, rhs.m_pointer);
	}

	template<std::derived_from<Base> Rhs>
	friend std::strong_ordering operator<=>(type_id const& lhs, type_id<Rhs> const& rhs)
	{
		return std::compare_three_way()(lhs.m_pointer, rhs.m_pointer);
	}

	template<typename>
	friend class type_id;
};

template<typename T>
constexpr type_id<T> type_of()
{
	return type_id<T>(&detail::type_id_instance<T>);
}

template<typename T>
constexpr type_id<T> type_of(T const&)
{
	return type_of<T>();
}

#define allio_TYPE_ID(T) \
	template ::allio::detail::type_id_object const ::allio::detail::type_id_instance<T>

} // namespace allio
