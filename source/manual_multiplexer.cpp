#include <allio/manual_multiplexer.hpp>

#include <allio/detail/assert.hpp>

#include "atomic_spin_hint.hpp"

#include <atomic>

using namespace allio;

result<manual_multiplexer::init_result> manual_multiplexer::init(manual_multiplexer::init_options const& options)
{
	return init_result(options);
}

manual_multiplexer::manual_multiplexer(init_result&& resources)
{
	init_options const& options = resources.options;

	m_relation_provider = options.relation_provider;
	m_defer_head = nullptr;
}

manual_multiplexer::~manual_multiplexer()
{
}

type_id<multiplexer> manual_multiplexer::get_type_id() const
{
	return type_of(*this);
}

result<multiplexer_handle_relation const*> manual_multiplexer::find_handle_relation(type_id<handle> const handle_type) const
{
	if (m_relation_provider != nullptr)
	{
		return m_relation_provider->find_multiplexer_handle_relation(type_of(*this), handle_type);
	}

	return allio_ERROR(error::unsupported_multiplexer_handle_relation);
}

result<void> manual_multiplexer::submit(deadline const deadline)
{
	return {};
}

result<void> manual_multiplexer::poll(deadline const deadline)
{
	auto defer_head = m_defer_head.load(std::memory_order_acquire);

	if (defer_head == nullptr)
	{
		if (m_wait_event == nullptr || deadline == deadline::instant())
		{
			return {};
		}

		allio_TRYV(m_wait_event->wait(deadline));
	}

	defer_head = m_defer_head.exchange(nullptr, std::memory_order_acq_rel);
	allio_ASSERT(defer_head != nullptr);

	defer_head = reverse_defer_list(defer_head);

	do
	{
		auto const storage = std::exchange(defer_head, defer_head->defer_next);
		set_status(*storage, async_operation_status::concluded);

		if (auto const listener = storage->get_listener())
		{
			listener->completed(*storage);
			listener->concluded(*storage);
		}
	}
	while (defer_head != nullptr);

	return {};
}

result<void> manual_multiplexer::submit_and_poll(deadline const deadline)
{
	return poll(deadline);
}

void manual_multiplexer::complete(async_operation_storage& storage, std::error_code const result)
{
	set_result(storage, result);

	auto defer_head = m_defer_head.load(std::memory_order_acquire);

	while (true)
	{
		storage.defer_next = defer_head;

		if (m_defer_head.compare_exchange_weak(defer_head, &storage,
			std::memory_order_release, std::memory_order_acquire))
		{
			break;
		}

		atomic_spin_hint();
	}

	set_status(storage, async_operation_status::completed);

	if (m_wait_event != nullptr)
	{
		m_wait_event->wake();
	}
}

manual_multiplexer::async_operation_storage* manual_multiplexer::reverse_defer_list(async_operation_storage* head)
{
	async_operation_storage* prev = nullptr;

	while (head != nullptr)
	{
		head = std::exchange(head->defer_next, std::exchange(prev, head));
	}

	return prev;
}

allio_TYPE_ID(manual_multiplexer);
