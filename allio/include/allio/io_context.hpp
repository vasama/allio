#pragma once

#include <allio/detail/execution.hpp>
#include <allio/detail/handles/event_handle.hpp>

#include <vsm/intrusive/list.hpp>

namespace allio {
namespace detail {

template<typename Context>
class io_scheduler
{
	
};

template<typename Multiplexer>
class io_context
{
	struct sender_link : vsm::intrusive::list_link {};

	class sender : sender_link
	{
		
	};

	Multiplexer m_multiplexer;

	vsm::intrusive::list<sender_link> m_list;
	basic_event_handle<Multiplexer*> m_event;

public:
	explicit io_context(Multiplexer multiplexer)
		: m_multiplexer(vsm_move(multiplexer))
		, m_event(&*m_multiplexer)
	{
	}
};

} // namespace detail
} // namespace allio
