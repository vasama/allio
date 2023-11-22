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
	vsm::result<bool> vsm_static_operator_invoke(Multiplexer&& m, Args&&... args)
		requires vsm::tag_invocable<poll_io_t, Multiplexer&&, Args&&...>
	{
		return vsm::tag_invoke(poll_io_t(), vsm_forward(m), vsm_forward(args)...);
	}
};
inline constexpr poll_io_t poll_io = {};


template<multiplexer Multiplexer>
struct _basic_multiplexer_handle
{
	class type
	{
		Multiplexer* m_multiplexer;

	public:
		using multiplexer_handle_concept = void;

		using multiplexer_type = Multiplexer;

		type(Multiplexer& multiplexer)
			: m_multiplexer(&multiplexer)
		{
		}

		operator Multiplexer&() const
		{
			return *m_multiplexer;
		}

		friend vsm::result<bool> tag_invoke(poll_io_t, type const& self, auto&&... args)
		{
			return poll_io(*self.m_multiplexer, vsm_forward(args)...);
		}
	};
};

template<multiplexer Multiplexer>
using basic_multiplexer_handle = typename _basic_multiplexer_handle<Multiplexer>::type;

template<multiplexer Multiplexer>
auto _multiplexer_handle(Multiplexer const&)
{
	if constexpr (requires { typename Multiplexer::handle_type; })
	{
		static_assert(multiplexer_handle<typename Multiplexer::handle_type>);
		return vsm_declval(typename Multiplexer::handle_type);
	}
	else
	{
		return vsm_declval(basic_multiplexer_handle<Multiplexer>);
	}
}

template<multiplexer_handle MultiplexerHandle>
MultiplexerHandle _multiplexer_handle(MultiplexerHandle const&);

template<typename Multiplexer>
using multiplexer_handle_t = decltype(_multiplexer_handle(vsm_declval(Multiplexer)));

} // namespace allio::detail
