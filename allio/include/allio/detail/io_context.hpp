#pragma once

#include <allio/detail/execution.hpp>
#include <allio/detail/handles/event_handle.hpp>

#include <vsm/concepts.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_ptr.hpp>
#include <vsm/utility.hpp>

namespace allio::detail {

template<typename Multiplexer>
class basic_io_context
{
	using multiplexer_type = multiplexer_t<Multiplexer>;

	Multiplexer m_multiplexer;
	basic_event_queue<multiplexer_type*> m_event_queue;
	vsm::atomic<bool> m_stop_requested = false;

public:
	explicit basic_io_context(Multiplexer multiplexer)
		: m_multiplexer(vsm_move(multiplexer))
		, m_event_queue(&*m_multiplexer)
	{
	}


	auto get_scheduler()
	{
		return m_event_queue.get_scheduler();
	}


	auto run()
	{
	}

private:
	void schedule(operation_base& operation)
	{
		m_list.push_back(&operation);
	}
};

} // namespace allio::detail
