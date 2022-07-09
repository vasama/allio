#include <allio/win32/iocp_multiplexer.hpp>

#include <allio/detail/assert.hpp>
#include <allio/static_multiplexer_handle_relation_provider.hpp>
#include <allio/win32/kernel_error.hpp>
#include <allio/win32/platform.hpp>

#include "../async_handle_types.hpp"

#include "error.hpp"
#include "kernel.hpp"

#include <array>
#include <span>

using namespace allio;
using namespace allio::win32;

allio_EXTERN_ASYNC_HANDLE_MULTIPLEXER_RELATIONS(iocp_multiplexer);

static DWORD get_timeout(deadline const deadline)
{
	return INFINITE;
}

void iocp_multiplexer::completion_port_deleter::release(HANDLE const completion_port)
{
	allio_VERIFY(CloseHandle(completion_port));
}

result<iocp_multiplexer::init_result> iocp_multiplexer::init(init_options const& options)
{
	allio_TRYV(kernel_init());

	allio_TRY(completion_port, [&]() -> result<unique_completion_port>
	{
		HANDLE const completion_port = CreateIoCompletionPort(
			INVALID_HANDLE_VALUE,
			NULL,
			0,
			options.thread_count);

		if (completion_port == NULL)
		{
			return allio_ERROR(get_last_error_code());
		}

		return { result_value, completion_port };
	}());

	result<init_result> result = { result_value };
	result->completion_port = std::move(completion_port);
	result->slot_count = 256;
	result->slots = std::make_unique<io_slot[]>(result->slot_count);
	result->slot_ring = std::make_unique<uint32_t[]>(result->slot_count);
	return result;
}

iocp_multiplexer::iocp_multiplexer(init_result&& resources)
{
	m_completion_port = resources.completion_port.release();

	m_slots = resources.slots.release();
	m_slot_count = resources.slot_count;

	m_slot_ring = resources.slot_ring.release();
	m_slot_ring_head = 0;
	m_slot_ring_tail = static_cast<size_t>(0) - m_slot_count;

	for (size_t i = 0; i < m_slot_count; ++i)
	{
		m_slot_ring[i] = i;
	}
}

iocp_multiplexer::~iocp_multiplexer()
{
	completion_port_deleter::release(m_completion_port);
}

type_id<multiplexer> iocp_multiplexer::get_type_id() const
{
	return type_of(*this);
}

result<multiplexer_handle_relation const*> iocp_multiplexer::find_handle_relation(type_id<handle> const handle_type) const
{
	return static_multiplexer_handle_relation_provider<iocp_multiplexer, async_handle_types>::find_handle_relation(handle_type);
}

result<void> iocp_multiplexer::submit(deadline const deadline)
{
	return {};
}

result<void> iocp_multiplexer::poll(deadline const deadline)
{
	LARGE_INTEGER timeout_integer;
	LARGE_INTEGER* timeout = nullptr;

	// Flush the synchronous completion queue.
	if (m_synchronous_completion_head != nullptr)
	{
		do
		{
			auto const storage = m_synchronous_completion_head;
			m_synchronous_completion_head = storage->m_next;

			auto const listener = storage->get_listener();
			allio_ASSERT(listener != nullptr);

			listener->completed(*storage);
			listener->concluded(*storage);
		}
		while (m_synchronous_completion_head != nullptr);

		timeout_integer.QuadPart = 0;
		timeout = &timeout_integer;
	}
	else
	{
		//TODO: implement deadline as timeout
	}


	FILE_IO_COMPLETION_INFORMATION entries[16];
	size_t entry_count = 0;

	if (1) //TODO: use the multiple completion syscall if necessary
	{
		FILE_IO_COMPLETION_INFORMATION& entry = entries[0];

		NTSTATUS const status = NtRemoveIoCompletion(
			m_completion_port,
			&entry.CompletionKey,
			&entry.CompletionValue,
			&entry.IoStatusBlock,
			timeout);

		if (status >= 0 && status != STATUS_TIMEOUT)
		{
			entry_count = 1;
		}
	}

	for (auto const& entry : std::span(entries, entry_count))
	{
		auto const storage = [&]() -> async_operation_storage*
		{
			io_slot& slot = io_slot::get(*reinterpret_cast<IO_STATUS_BLOCK*>(entry.CompletionValue));
			allio_ASSERT(slot.m_storage != nullptr);

			async_operation_storage* const storage = slot.m_storage;
			allio_ASSERT(std::exchange(storage->m_slot, nullptr) == &slot);

			free_io_slot(slot);

			return storage;
		}();

		std::error_code const result = entry.IoStatusBlock.Status >= 0
			? as_error_code(storage->set_information(entry.IoStatusBlock.Information))
			: std::error_code(static_cast<kernel_error>(entry.IoStatusBlock.Status));

		set_result(*storage, result);
		set_status(*storage,
			async_operation_status::completed |
			async_operation_status::concluded);

		if (auto const listener = storage->get_listener())
		{
			listener->completed(*storage);
			listener->concluded(*storage);
		}
	}

	return {};
}

result<void> iocp_multiplexer::submit_and_poll(deadline const deadline)
{
	return poll(deadline);
}

static result<void> set_completion_information(HANDLE const handle, HANDLE const port, void* const key)
{
#if 0
	if (!CreateIoCompletionPort(handle, port, 0, 1))
	{
		return allio_ERROR(std::error_code(GetLastError(), std::system_category()));
	}
#else
	IO_STATUS_BLOCK isb = make_io_status_block();

	FILE_COMPLETION_INFORMATION fci =
	{
		.Port = port,
		.Key = key,
	};

	NTSTATUS status = NtSetInformationFile(
		handle,
		&isb,
		&fci,
		sizeof(fci),
		port != nullptr
			? FileCompletionInformation
			: FileReplaceCompletionInformation);

	if (status == STATUS_PENDING)
	{
		status = io_wait(handle, &isb, deadline());
	}

	if (status < 0)
	{
		return allio_ERROR(static_cast<kernel_error>(status));
	}
#endif

	return {};
}

result<iocp_multiplexer::handle_flags> iocp_multiplexer::register_native_handle(native_platform_handle const handle)
{
	HANDLE const h = unwrap_handle(handle);
	allio_TRYV(set_completion_information(h, m_completion_port, nullptr));

	handle_flags flags = handle_flags::none;

	if (SetFileCompletionNotificationModes(h,
		FILE_SKIP_COMPLETION_PORT_ON_SUCCESS | FILE_SKIP_SET_EVENT_ON_HANDLE))
	{
		flags |= handle_flags::supports_synchronous_completion;
	}

	return flags;
}

result<void> iocp_multiplexer::deregister_native_handle(native_platform_handle const handle)
{
	HANDLE const h = unwrap_handle(handle);
	allio_TRYV(set_completion_information(h, NULL, nullptr));

	return {};
}

result<size_t> iocp_multiplexer::acquire_io_slots_internal(size_t const count)
{
	size_t const head = m_slot_ring_head;
	size_t const tail = m_slot_ring_tail;

	if (head - tail < count)
	{
		return allio_ERROR(error::too_many_concurrent_async_operations);
	}

	m_slot_ring_tail = tail + count;
	return tail;
}

void iocp_multiplexer::release_io_slots_internal(size_t const count)
{
	m_slot_ring_tail -= count;
}

void iocp_multiplexer::free_io_slot(io_slot& slot)
{
	m_slot_ring[m_slot_ring_head++ & m_slot_count - 1] = &slot - m_slots;
}

void iocp_multiplexer::post_synchronous_completion(async_operation_storage& storage, uintptr_t const information)
{
	std::error_code const result = as_error_code(storage.set_information(information));

	if (io_slot* const slot = storage.m_slot)
	{
		free_io_slot(*slot);
	}

	set_result(storage, result);
	set_status(storage,
		async_operation_status::completed |
		async_operation_status::concluded);

	if (storage.get_listener() != nullptr)
	{
		storage.m_next = nullptr;
		if (m_synchronous_completion_head != nullptr)
		{
			allio_ASSERT(m_synchronous_completion_tail != nullptr);
			allio_ASSERT(m_synchronous_completion_tail->m_next == nullptr);
			m_synchronous_completion_tail->m_next = &storage;
		}
		else
		{
			m_synchronous_completion_head = &storage;
		}
		m_synchronous_completion_tail = &storage;
	}
}

result<void> iocp_multiplexer::cancel(native_platform_handle const handle, async_operation_storage& storage)
{
	//TODO: is async cancel a possibility here? io_cancel waits.

	NTSTATUS const status = io_cancel(unwrap_handle(handle), &storage.m_slot->io_status_block());

	if (status < 0)
	{
		return allio_ERROR(static_cast<kernel_error>(status));
	}

	return {};
}
