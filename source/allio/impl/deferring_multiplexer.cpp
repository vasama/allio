#include <allio/deferring_multiplexer.hpp>

using namespace allio;

void deferring_multiplexer::post(async_operation_storage& operation, async_operation_status const status)
{
	allio_ASSERT(status > operation.get_status());
	if (operation.get_listener() != nullptr)
	{
		if (operation.m_next.ptr() == &operation)
		{
			operation.m_next.set_ptr(nullptr);
			if (m_head != nullptr)
			{
				m_tail->m_next.set_ptr(&operation);
			}
			else
			{
				m_head = &operation;
			}
			m_tail = &operation;
		}
		else
		{
			// If the operation is already in a deferral list,
			// then this multiplexer's list better not be empty.
			allio_ASSERT(m_head != nullptr);
		}
	}
	set_status(operation, status);
}

void deferring_multiplexer::flush()
{
	// The two nested loops are to also flush any deferrals posted by the listener callbacks.
	for (async_operation_storage* head; (head = std::exchange(m_head, nullptr)) != nullptr;)
	{
		do
		{
			async_operation_storage* const operation = std::exchange(head, head->m_next.ptr());
			async_operation_listener* const listener = operation->get_listener();

			async_operation_status const old_status = operation->m_next.tag();
			async_operation_status const new_status = operation->get_status();

			allio_ASSERT(new_status > old_status);
			operation->m_next.set(operation, new_status);

			using integer_type = std::underlying_type_t<async_operation_status>;
			switch (static_cast<async_operation_status>(static_cast<integer_type>(old_status) + 1))
			{
			case async_operation_status::submitted:
				listener->submitted(*operation);
				if (new_status == async_operation_status::submitted)
				{
					break;
				}
				[[fallthrough]];

			case async_operation_status::completed:
				listener->completed(*operation);
				if (new_status == async_operation_status::completed)
				{
					break;
				}
				[[fallthrough]];

			case async_operation_status::concluded:
				listener->concluded(*operation);
				break;
			}
		}
		while (head != nullptr);
	}
}
