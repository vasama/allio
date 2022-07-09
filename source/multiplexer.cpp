#include <allio/multiplexer.hpp>

#include <allio/detail/assert.hpp>

using namespace allio;

static size_t alloca_size(storage_requirements const& r)
{
	return r.size + r.alignment;
}

static storage_ptr alloca_data(storage_requirements const& r, void* buffer)
{
	size_t const offset = reinterpret_cast<uintptr_t>(buffer) & r.alignment - 1;
	return storage_ptr(reinterpret_cast<std::byte*>(buffer) + offset, r.size);
}

#define allio_ALLOCA_STORAGE(requirements) \
	alloca_data(requirements, allio_detail_ALLOCA(alloca_size(requirements)))


result<multiplexer_handle_relation const*> multiplexer::find_multiplexer_handle_relation(
	type_id<multiplexer> const multiplexer_type, type_id<handle> const handle_type) const
{
	allio_ASSERT(multiplexer_type == get_type_id());
	return find_handle_relation(handle_type);
}

result<async_operation*> multiplexer::construct(async_operation_descriptor const& descriptor, storage_ptr const storage, async_operation_parameters const& arguments, async_operation_listener* const listener)
{
	return descriptor.construct(*this, storage, arguments, listener);
}

result<void> multiplexer::start(async_operation_descriptor const& descriptor, async_operation& operation)
{
	return descriptor.start(*this, operation);
}

result<async_operation*> multiplexer::construct_and_start(async_operation_descriptor const& descriptor, storage_ptr const storage, async_operation_parameters const& arguments, async_operation_listener* const listener)
{
	return descriptor.construct_and_start(*this, storage, arguments, listener);
}

result<void> multiplexer::cancel(async_operation_descriptor const& descriptor, async_operation& operation)
{
	return descriptor.cancel(*this, operation);
}

result<void> multiplexer::submit(deadline const deadline)
{
	return {};
}

result<void> multiplexer::submit_and_poll(deadline const deadline)
{
	allio_TRYV(submit(deadline));
	allio_TRYV(poll(deadline));
	return {};
}

result<void> multiplexer::block(multiplexer_handle_relation const& relation, async_operation_descriptor const& descriptor, async_operation_parameters const& arguments)
{
	return do_block(descriptor, allio_ALLOCA_STORAGE(relation.operation_storage_requirements), arguments);
}

result<void> multiplexer::do_block(async_operation_descriptor const& descriptor, storage_ptr const storage, async_operation_parameters const& arguments)
{
	allio_TRY(operation, construct_and_start(descriptor, storage, arguments, nullptr));

	while (!operation->is_concluded())
	{
		allio_TRYV(poll());
	}

	if (std::error_code const result = operation->get_result())
	{
		return allio_ERROR(result);
	}

	return {};
}
