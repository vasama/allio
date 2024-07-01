#pragma once

#include <exec/task.hpp>

namespace allio::detail {

template<typename MultiplexerHandle>
class task_context : public exec::default_task_context<void>
{
	MultiplexerHandle m_multiplexer_handle;

public:
	template<typename ParentPromise>
	explicit task_context(exec::__task::__parent_promise_t tag, ParentPromise& parent)
		: exec::default_task_context<void>(tag, parent)
		, m_multiplexer_handle(get_multiplexer(ex::get_env(parent)))
	{
	}

	template<stdexec::scheduler Scheduler, std::convertible_to<MultiplexerHandle> Multiplexer>
	explicit task_context(Scheduler&& scheduler, Multiplexer&& multiplexer)
		: exec::default_task_context<void>(vsm_forward(scheduler))
		, m_multiplexer_handle(vsm_forward(multiplexer))
	{
	}

	friend MultiplexerHandle const& tag_invoke(get_multiplexer_t, task_context const& self) noexcept
	{
		return self.m_multiplexer_handle;
	}

	template<typename ThisPromise>
	using promise_context_t = task_context;

	template<typename ThisPromise, typename ParentPromise>
	using awaiter_context_t = exec::__task::__default_awaiter_context<ParentPromise>;
};

template<typename T, typename MultiplexerHandle>
using basic_task = exec::basic_task<T, detail::task_context<MultiplexerHandle>>;

} // namespace allio::detail
