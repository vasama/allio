#pragma once

#include <allio/detail/execution.hpp>

#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

namespace allio::detail {

struct get_multiplexer_t : ex::__query<get_multiplexer_t>
{
	friend constexpr bool tag_invoke(ex::forwarding_query_t, get_multiplexer_t const&) noexcept
	{
		return true;
	}
	
	template<typename Env>
		requires vsm::tag_invocable<get_multiplexer_t, Env const&>
	auto vsm_static_operator_invoke(Env const& env) noexcept
		-> vsm::tag_invoke_result_t<get_multiplexer_t, Env const&>
	{
		static_assert(vsm::nothrow_tag_invocable<get_multiplexer_t, Env const&>);
		return vsm::tag_invoke(get_multiplexer_t(), env);
	}

	auto operator()() const noexcept
	{
		return ex::read(*this);
	}
};
inline constexpr get_multiplexer_t get_multiplexer = {};

template<typename Receiver>
using current_multiplexer_t = std::decay_t<std::invoke_result_t<get_multiplexer_t, ex::env_of_t<Receiver>>>;

} // namespace allio::detail
