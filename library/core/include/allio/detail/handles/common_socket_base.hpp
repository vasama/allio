#pragma once

#include <allio/detail/object.hpp>
#include <allio/network.hpp>

namespace allio::detail {

template<object BaseObject>
struct common_socket_base_t : BaseObject
{
	using base_type = BaseObject;

	template<typename Handle, typename Traits>
	struct facade : base_type::template facade<Handle, Traits>
	{
		[[nodiscard]] network_address_kind address_kind() const
		{
			return Handle::object_type::get_address_kind(
				static_cast<Handle const&>(*this).native());
		}
	};
};

} // namespace allio::detail;
