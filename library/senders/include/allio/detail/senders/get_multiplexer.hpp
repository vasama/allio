#pragma once

#include <allio/detail/execution.hpp>

#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

namespace allio::detail {

struct get_multiplexer_t : ex::__query<get_multiplexer_t>
{
	using ex::__query<get_multiplexer_t>::operator();

	[[nodiscard]] constexpr auto operator()() const noexcept;

	template<typename Env>
	[[nodiscard]] static constexpr void __validate() noexcept
	{
		static_assert(ex::__nothrow_callable<get_multiplexer_t, Env const&>);
		using multiplexer_handle_type = ex::__call_result_t<get_multiplexer_t, Env const &>;
		static_assert(multiplexer_handle<ex::__decay_t<multiplexer_handle_type>>);
	}

	[[nodiscard]] static constexpr bool query(ex::forwarding_query_t) noexcept
	{
		return true;
	}
};

inline constexpr auto get_multiplexer_t::operator()() const noexcept
{
	return ex::read_env(get_multiplexer_t());
}

inline constexpr get_multiplexer_t get_multiplexer = {};

template<typename Env>
using env_multiplexer_handle_t = std::remove_cvref_t<std::invoke_result_t<get_multiplexer_t, Env>>;

} // namespace allio::detail
