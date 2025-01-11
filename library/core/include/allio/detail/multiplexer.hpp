#pragma once

#include <vsm/result.hpp>
#include <vsm/tag_invoke.hpp>
#include <vsm/utility.hpp>

#include <concepts>

namespace allio::detail {

template<typename Multiplexer>
concept multiplexer =
	std::is_void_v<typename Multiplexer::multiplexer_concept>;

template<typename MultiplexerHandle>
concept multiplexer_handle =
	std::is_void_v<typename MultiplexerHandle::multiplexer_handle_concept> &&
	multiplexer<typename MultiplexerHandle::multiplexer_type>;

template<typename MultiplexerHandle>
concept optional_multiplexer_handle =
	std::is_void_v<MultiplexerHandle> ||
	multiplexer_handle<MultiplexerHandle>;


struct poll_io_t
{
	template<typename Multiplexer, typename... Args>
		requires vsm::tag_invocable<poll_io_t, Multiplexer&&, Args&&...>
	[[nodiscard]] vsm_static_operator vsm::result<bool> operator()(
		Multiplexer&& m,
		Args&&... args) vsm_static_operator_const
	{
		return vsm::tag_invoke(poll_io_t(), vsm_forward(m), vsm_forward(args)...);
	}
};
inline constexpr poll_io_t poll_io = {};

} // namespace allio::detail
