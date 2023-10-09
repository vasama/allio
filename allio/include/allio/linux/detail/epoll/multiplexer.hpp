#pragma once

#include <allio/linux/detail/unique_fd.hpp>

#include <vsm/intrusive/list.hpp>



namespace allio::detail {

class epoll_multiplexer
{
	struct poll_slot_base : vsm::intrusive::list_link {};

public:
	struct operation_type : vsm::intrusive::list_link
	{
		
	};

private:
	struct operation_list
	{
		vsm::intrusive_list<operation_type> list;
	};

public:
	struct connector_type
	{
		size_t index;
	};


	template<short Events>
	class poll_slot : poll_slot_base
	{
		friend epoll_multiplexer;
	};

private:
	unique_fd m_epoll;

public:
};

struct example
{
	poll_slot<POLLIN> slot;

	vsm::result<io_result> submit(M& m, C const& c)
	{
		return m.poll(c, slot, [&](poll_events_t) -> vsm::result<io_result>
		{
			
		});
	}
};

} // namespace allio::detail
