#pragma once

#include <allio/opaque_handle.h>

#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

namespace allio::detail {

/// @brief ABI stable native handle type for asynchronous polling across ABI boundaries such as
///        dynamically linked library interfaces. Instead of exposing allio types in your library
///        ABI, return an opaque_handle_v1 instead and wrap it in a @ref opaque_handle before
///        passing it to the user.
using opaque_handle_v1 = allio_opaque_handle_v1;


template<typename Version>
struct get_opaque_handle_t
{
	template<typename Handle>
	vsm::result<Version> vsm_static_operator_invoke(Handle const& h) noexcept
		requires vsm::tag_invocable<get_opaque_handle_t, Handle const&>
	{
		return vsm::tag_invoke(get_opaque_handle_t(), h);
	}
};

template<typename Version>
inline constexpr get_opaque_handle_t get_opaque_handle = {};

} // namespace allio::detail
