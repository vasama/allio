#pragma once

#include <allio/multiplexer.hpp>

#include <vsm/result.hpp>
#include <vsm/utility.hpp>

namespace allio {

template<typename Pointer>
class basic_multiplexer_pointer_handle
{
	Pointer m_multiplexer;

public:
	explicit basic_multiplexer_pointer_handle(Pointer multiplexer)
		: m_multiplexer(vsm_move(multiplexer))
	{
	}

	template<typename H, typename S>
	friend vsm::result<void> tag_invoke(submit_t const submit, H& handle, S& state)
		requires requires { submit(*m_multiplexer, handle, state); }
	{
		return submit(*m_multiplexer, handle, state);
	}

	template<typename H, typename S>
	friend vsm::result<void> tag_invoke(cancel_t const cancel, H& handle, S& state)
		requires requires { cancel(*m_multiplexer, handle, state); }
	{
		return cancel(*m_multiplexer, handle, state);
	}
};

template<typename Multiplexer>
using multiplexer_pointer_handle = basic_multiplexer_pointer_handle<Multiplexer*>;

} // namespace allio
