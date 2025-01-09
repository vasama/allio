#pragma once

#include <allio/abi.h>

#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

#include <memory>

namespace allio::detail {

using opaque_handle = allio_abi_handle;
using opaque_handle_functions = allio_abi_handle_functions;


struct opaque_handle_deleter
{
	vsm_static_operator void operator()(opaque_handle* const handle) vsm_static_operator_const
	{
		handle->functions->close(handle);
	}
};
using unique_opaque_handle = std::unique_ptr<opaque_handle, opaque_handle_deleter>;


template<typename Version>
struct get_opaque_handle_t
{
	template<typename Handle>
	vsm_static_operator vsm::result<Version>
	operator()(Handle const& h) vsm_static_operator_const noexcept
		requires vsm::tag_invocable<get_opaque_handle_t, Handle const&>
	{
		return vsm::tag_invoke(get_opaque_handle_t(), h);
	}
};

template<typename Version>
inline constexpr get_opaque_handle_t get_opaque_handle = {};

} // namespace allio::detail
