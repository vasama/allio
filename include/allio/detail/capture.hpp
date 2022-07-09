#pragma once

#include <allio/result.hpp>

#include <concepts>

namespace allio::detail {

template<typename Base, typename Parameter, typename T>
struct capture_traits
{
	static_assert(sizeof(Base) == 0,
		"Callable is not convertible to a function pointer of an applicable signature.");
};

template<typename Base, typename Parameter, typename Callable, std::derived_from<Base> Storage, typename CallbackParameter>
struct capture_traits<Base, Parameter, result<void>(Callable::*)(Storage& storage, CallbackParameter argument) const>
{
	static result<void> callback(Base& storage, Parameter argument)
	{
		return Callable()(static_cast<Storage&>(storage), static_cast<Parameter&&>(argument));
	}
};

template<typename Base, typename Parameter, typename Callable, std::derived_from<Base> Storage, typename CallbackParameter>
struct capture_traits<Base, Parameter, void(Callable::*)(Storage& storage, CallbackParameter argument) const>
{
	static result<void> callback(Base& storage, Parameter argument)
	{
		Callable()(static_cast<Storage&>(storage), static_cast<Parameter&&>(argument));
		return {};
	}
};

} // namespace allio::detail
