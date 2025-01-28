#pragma once

#include <allio/detail/execution.hpp>

#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

namespace allio::detail {

struct get_multiplexer_t : ex::__query<get_multiplexer_t>
{
	static constexpr bool query(ex::forwarding_query_t) noexcept
	{
		return true;
	}

	template<typename Env>
		requires ex::tag_invocable<get_multiplexer_t, Env const&>
	[[nodiscard]] vsm_static_operator auto operator()(
		Env const& env) vsm_static_operator_const noexcept
		-> ex::tag_invoke_result_t<get_multiplexer_t, Env const&>
	{
		static_assert(ex::nothrow_tag_invocable<get_multiplexer_t, Env const&>);
		return ex::tag_invoke(get_multiplexer_t(), env);
	}

	template<typename = get_multiplexer_t>
	[[nodiscard]] vsm_static_operator auto operator()() vsm_static_operator_const noexcept
	{
		return ex::read_env(get_multiplexer_t());
	}
};
inline constexpr get_multiplexer_t get_multiplexer = {};

template<typename Env>
using env_multiplexer_handle_t = std::remove_cvref_t<std::invoke_result_t<get_multiplexer_t, Env>>;

} // namespace allio::detail
