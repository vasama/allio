#pragma once

#include <exec/task.hpp>

namespace allio::detail {

template<typename T, typename MultiplexerHandle>
class task_context : public exec::default_task_context<T>
{
	using default_context = exec::default_task_context<T>;

	vsm_no_unique_address MultiplexerHandle m_multiplexer_handle;

public:
	template<typename ParentPromise>
	explicit task_context(ParentPromise& parent)
		: default_context(parent)
		, m_multiplexer_handle(get_multiplexer(ex::get_env(parent)))
	{
	}

#if 0
	template<ex::scheduler Scheduler, std::convertible_to<MultiplexerHandle> Multiplexer>
	explicit task_context(Scheduler&& scheduler, Multiplexer&& multiplexer)
		: default_context(vsm_forward(scheduler))
		, m_multiplexer_handle(vsm_forward(multiplexer))
	{
	}
#endif

	using default_context::query;

	[[nodiscard]] constexpr MultiplexerHandle const& query(get_multiplexer_t) const noexcept
	{
		return m_multiplexer_handle;
	}

	template<typename ThisPromise>
	using promise_context_t = task_context;
};

template<typename T, typename MultiplexerHandle>
using basic_task = exec::basic_task<T, detail::task_context<T, MultiplexerHandle>>;

} // namespace allio::detail
