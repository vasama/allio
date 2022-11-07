#pragma once

#include <allio/detail/linear.hpp>
#include <allio/multiplexer.hpp>

#include <mutex>

namespace allio {

class synchronized_multiplexer final : public multiplexer
{
	unique_multiplexer_ptr m_multiplexer;
	std::mutex m_mutex;

public:
	explicit synchronized_multiplexer(unique_multiplexer_ptr multiplexer)
		: m_multiplexer(static_cast<unique_multiplexer_ptr&&>(multiplexer))
	{
	}

	type_id<multiplexer> get_type_id() const override;

	result<multiplexer_handle_relation const*> find_handle_relation(type_id<handle> handle_type) const override;

	result<async_operation*> construct(async_operation_descriptor const& descriptor, storage_ptr storage, async_operation_parameters const& arguments) override;
	result<void> start(async_operation_descriptor const& descriptor, async_operation& operation, bool submit) override;
	result<async_operation*> construct_and_start(async_operation_descriptor const& descriptor, storage_ptr storage, async_operation_parameters const& arguments, bool submit) override;
	result<void> cancel(async_operation_descriptor const& descriptor, async_operation& operation, bool submit) override;

	result<void> submit(deadline deadline) override;
	result<void> poll(deadline deadline) override;
	result<void> submit_and_poll(deadline deadline) override;
};

} // namespace allio
