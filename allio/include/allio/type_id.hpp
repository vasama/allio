#pragma once

#include <compare>
#include <type_traits>

namespace allio {

template<typename T>
struct type_id_traits;

namespace detail {

struct type_id_base;

struct type_id_object {};

template<typename T>
using type_id_object_type = typename type_id_traits<T>::object_type;

template<typename T>
inline type_id_object_type<T> const type_id_instance = {};

} // namespace detail

template<typename T>
struct type_id_traits
{
	using object_type = detail::type_id_object;

	static constexpr object_type const* get_object()
	{
		return &detail::type_id_instance<T>;
	}
};


template<typename Base = detail::type_id_base>
class type_id;

template<>
class type_id<detail::type_id_base>
{
	void const* m_pointer;

public:
	template<typename T>
	constexpr type_id(type_id<T> const& other)
		: m_pointer(other.m_pointer)
	{
	}

	void const* get_object() const
	{
		return m_pointer;
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
	using object_type = detail::type_id_object_type<Base>;

	object_type const* m_pointer;

public:
	constexpr type_id(object_type const* const pointer)
		: m_pointer(pointer)
	{
	}

	template<std::derived_from<Base> Derived>
	//	requires std::convertible_to_v<detail::type_id_object_type<Derived> const*, object_type const*>
	constexpr type_id(type_id<Derived> const& derived)
		: m_pointer(derived.m_pointer)
	{
	}

	constexpr object_type const* get_object() const
	{
		return m_pointer;
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
	return type_id<T>(type_id_traits<T>::get_object());
}

template<typename T>
constexpr type_id<T> type_of(T const&)
{
	return type_of<T>();
}

#define allio_type_id(...) \
	template ::allio::detail::type_id_object const ::allio::detail::type_id_instance<__VA_ARGS__>; \
	static_assert(::std::is_same_v<::allio::detail::type_id_object_type<__VA_ARGS__>, ::allio::detail::type_id_object>)

} // namespace allio
