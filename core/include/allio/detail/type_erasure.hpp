#pragma once

#include <vsm/concepts.hpp>
#include <vsm/tag_invoke.hpp>

namespace allio::detail {

template<typename T>
struct virtual_object
{
	T& object;

	virtual_object(T& object)
		: object(object)
	{
	}

	template<vsm::inherited_from<T> U>
	virtual_object(virtual_object<U> const& object)
		: object(object.object)
	{
	}
};

template<typename T>
virtual_object(T&) -> virtual_object<T>;

template<typename Signature>
struct virtual_function;

template<typename Argument>
Argument&& virtual_unwrap(Argument&& argument)
{
	return static_cast<Argument&&>(argument);
}

template<typename Object>
Object& virtual_unwrap(virtual_object<Object> const& object)
{
	return object.object;
}

template<typename R, typename CPO, typename... Ps>
struct virtual_function<R(CPO, Ps...)>
{
	R(*function)(Ps...);

	template<typename... Ts>
	explicit virtual_function(vsm::tag<Ts>...)
		: function(function_instance<Ts...>)
	{
	}

	template<std::convertible_to<Ps>... Args>
	R operator()(CPO, Args&&... args) const
	{
		return (*function)(vsm_forward(args)...);
	}

	template<typename... Ts>
	static R function_instance(Ps... args)
	{
		return CPO()(vsm_move(args)...);
	}
};

template<typename... Signatures>
struct virtual_table : virtual_function<Signatures>...
{
	template<typename... Ts>
	explicit virtual_table(vsm::tag<Ts>... ts)
		: virtual_function<Signatures>(ts...)...
	{
	}

	using virtual_function<Signatures>::operator()...
};

template<typename VirtualTable, typename... Ts>
inline constexpr VirtualTable virtual_table_for;

template<typename... Signatures, typename... Ts>
inline constexpr virtual_table<Signatures...> virtual_table_for<virtual_table<Signatures...>, Ts...>(vsm::tag<Ts>{}...);

} // namespace allio::detail
