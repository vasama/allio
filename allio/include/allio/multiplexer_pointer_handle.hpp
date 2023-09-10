#pragma once

#include <allio/multiplexer.hpp>

#include <vsm/result.hpp>
#include <vsm/utility.hpp>

#include <memory>

namespace allio {

template<typename Pointer>
class basic_multiplexer_pointer_handle
{
	Pointer m_multiplexer;

public:
	using value_type = std::remove_cvref_t<decltype(*vsm_declval(Pointer))>;

	explicit basic_multiplexer_pointer_handle(Pointer multiplexer)
		: m_multiplexer(vsm_move(multiplexer))
	{
	}

	template<typename CPO, typename... Args>
	friend auto tag_invoke(CPO cpo, basic_multiplexer_pointer_handle& h, Args&&... args)
		-> decltype(cpo(*h.m_multiplexer, vsm_forward(args)...))
	{
	}
};

template<typename Multiplexer>
using multiplexer_pointer_handle = basic_multiplexer_pointer_handle<Multiplexer*>;

} // namespace allio
