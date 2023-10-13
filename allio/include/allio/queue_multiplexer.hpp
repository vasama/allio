#pragma once

#include <vsm/intrusive/list.hpp>

namespace allio {

template<typename Multiplexer>
class queue_multiplexer
{
	struct operation_base : vsm::intrusive::list_link {};

public:
	using connector_type = typename Multiplexer::connector_type;

	class operation_type
		: operation_base
		, public Multiplexer::operation_type
	{
	};

private:
	Multiplexer m_multiplexer;
	vsm::intrusive::list<operation_base> m_queue;

	template<typename H, typename C, std::derived_from<operation_type> S, typename R>
	friend io_result2 tag_invoke(submit_io_t, queue_multiplexer& m, H&& h, C&& c, S&& s, R&& r)
	{
		operation_type& my_operation = s;
		typename Multiplexer::operation_type& base_operation = my_operation;

		io_result2 r = submit_io(m.m_multiplexer, c, s, r);
		if (!r && r.error() == std::error_code(error::too_many_concurrent_async_operations))
		{
			m_queue.push_back(&my_operation);
			r = std::nullopt;
		}
		return r;
	}

	template<typename H, typename C, std::derived_from<operation_type> S, typename R>
	friend io_result2 tag_invoke(notify_io_t, queue_multiplexer& m, H&& h, C&& c, S&& s, R&& r, io_status const status)
	{
		operation_type& my_operation = s;
		typename Multiplexer::operation_type& base_operation = my_operation;

		io_result2 r = notify_io(m.m_multiplexer, c, s, r, status);
		if (!r && r.error() == std::error_code(error::too_many_concurrent_async_operations))
		{
			m_queue.push_back(&my_operation);
			r = std::nullopt;
		}
		return r;
	}

private:
	static io_result2 handle_result()
};

} // namespace allio
