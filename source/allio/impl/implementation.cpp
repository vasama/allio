#include <allio/implementation.hpp>

#include <algorithm>

using namespace allio;
using namespace allio::detail;

result<multiplexer_handle_relation const*> detail::find_handle_relation(
	std::span<const handle_relation_info> const handle_types, type_id<handle> const handle_type)
{
	auto const it = std::find_if(handle_types.begin(), handle_types.end(), [&](auto const& info)
	{
		return info.type == handle_type;
	});

	if (it != handle_types.end() && it->relation != nullptr)
	{
		return it->relation;
	}

	return allio_ERROR(error::unsupported_multiplexer_handle_relation);
}
