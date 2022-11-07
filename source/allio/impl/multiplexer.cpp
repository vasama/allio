#include <allio/multiplexer.hpp>

#include <allio/detail/assert.hpp>

using namespace allio;

result<multiplexer_handle_relation const*> multiplexer::find_multiplexer_handle_relation(
	type_id<multiplexer> const multiplexer_type, type_id<handle> const handle_type) const
{
	allio_ASSERT(multiplexer_type == get_type_id());
	return find_handle_relation(handle_type);
}

result<async_operation_ptr> multiplexer::construct(async_operation_descriptor const& descriptor, storage_ptr const storage, async_operation_parameters const& arguments, async_operation_listener* const listener)
{
	return descriptor.construct(*this, storage, arguments, listener);
}

result<void> multiplexer::start(async_operation_descriptor const& descriptor, async_operation& operation)
{
	return descriptor.start(*this, operation);
}

result<async_operation_ptr> multiplexer::construct_and_start(async_operation_descriptor const& descriptor, storage_ptr const storage, async_operation_parameters const& arguments, async_operation_listener* const listener)
{
	return descriptor.construct_and_start(*this, storage, arguments, listener);
}

result<void> multiplexer::cancel(async_operation_descriptor const& descriptor, async_operation& operation)
{
	return descriptor.cancel(*this, operation);
}

result<void> multiplexer::block(async_operation_descriptor const& descriptor, async_operation_parameters const& arguments)
{
	return descriptor.block(*this, arguments);
}

#if 0
result<multiplexer::statistics> multiplexer::submit(deadline const deadline)
{
	return {};
}

result<multiplexer::statistics> multiplexer::submit_and_poll(deadline deadline)
{
	deadline = deadline.start();
	return gather_statistics([&](statistics& statistics) -> result<void>
	{
		allio_TRY(submit_statistics, submit(deadline));
		add_statistics(statistics, submit_statistics);

		allio_TRY(poll_statistics, poll(deadline));
		add_statistics(statistics, poll_statistics);

		return {};
	});
}
#endif
