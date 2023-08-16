#include <allio/synchronized_multiplexer.hpp>

using namespace allio;

type_id<multiplexer> synchronized_multiplexer::get_type_id() const
{
	return type_of(*this);
}

vsm::result<multiplexer_handle_relation const*> synchronized_multiplexer::find_handle_relation(type_id<handle> const handle_type) const
{
	return m_multiplexer->find_handle_relation(handle_type);
}

vsm::result<async_operation_ptr> synchronized_multiplexer::construct(async_operation_descriptor const& descriptor, storage_ptr const storage, async_operation_parameters const& arguments)
{
	std::lock_guard lock(m_mutex);
	return m_multiplexer->construct(descriptor, storage, arguments);
}

vsm::result<void> synchronized_multiplexer::start(async_operation_descriptor const& descriptor, async_operation& operation)
{
	std::lock_guard lock(m_mutex);
	return m_multiplexer->start(descriptor, operation);
}

vsm::result<async_operation_ptr> synchronized_multiplexer::construct_and_start(async_operation_descriptor const& descriptor, storage_ptr const storage, async_operation_parameters const& arguments, async_operation_listener* const listener)
{
	std::lock_guard lock(m_mutex);
	return m_multiplexer->construct_and_start(descriptor, storage, arguments, listener);
}

vsm::result<void> synchronized_multiplexer::cancel(async_operation_descriptor const& descriptor, async_operation& operation)
{
	std::lock_guard lock(m_mutex);
	return m_multiplexer->cancel(descriptor, operation);
}

vsm::result<void> synchronized_multiplexer::submit(deadline const deadline)
{
	std::lock_guard lock(m_mutex);
	return m_multiplexer->submit(deadline);
}

vsm::result<void> synchronized_multiplexer::poll(deadline const deadline)
{
	std::lock_guard lock(m_mutex);
	return m_multiplexer->poll(deadline);
}

vsm::result<void> synchronized_multiplexer::submit_and_poll(deadline const deadline)
{
	std::lock_guard lock(m_mutex);
	return m_multiplexer->submit_and_poll(deadline);
}
