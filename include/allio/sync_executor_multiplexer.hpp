#pragma once

#include <allio/multiplexer.hpp>

namespace allio {

enum class sync_executor_slot : uintptr_t {};

class sync_executor
{
public:
	virtual storage_requirements get_operation_storage_requirements() const = 0;
	virtual result<sync_executor_slot> schedule(async_operation_parameters const& args) = 0;
	virtual result<void> cancel(sync_executor_slot slot) = 0;
};

class sync_executor_multiplexer : public multiplexer
{
	multiplexer* m_multiplexer;
	sync_executor* m_executor;

public:
	explicit sync_executor_multiplexer(multiplexer* const multiplexer)
		: m_multiplexer(multiplexer)
	{
	}
};

} // namespace allio
