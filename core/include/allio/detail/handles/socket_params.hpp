#pragma once

#include <allio/detail/parameters.hpp>

namespace allio::detail {

template<bool Void>
struct _socket_params;

template<>
struct _socket_params<0>
{
	template<typename Base, typename SecurityContext>
	struct type : Base
	{
		SecurityContext const* security_context;

		void set_argument(SecurityContext const& value)
		{
			security_context = &value;
		}
	};
};

template<>
struct _socket_params<1>
{
	template<typename Base, typename SecurityContext>
	using type = Base;
};

template<typename Base, typename SecurityContext>
using socket_params = typename _socket_params<std::is_void_v<SecurityContext>>::template type<Base, SecurityContext>;

} // namespace allio::detail
