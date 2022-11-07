#pragma once

#include <allio/deadline.hpp>

namespace allio {

class step_deadline
{
	deadline m_deadline;

public:
	step_deadline(deadline const deadline)
		: m_deadline(deadline)
	{
	}

	result<deadline> step()
	{
		if (m_deadline.is_relative())
		{
			deadline const step = m_deadline;
			m_deadline = step.start();
			return step;
		}

		auto const now = deadline::clock().now();
		auto const end = m_deadline.absolute();

		if (now >= end)
		{
			return allio_ERROR(error::async_operation_timed_out);
		}

		return result<deadline>(result_value, end - now);
	}
};

} // namespace allio
