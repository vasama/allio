#include <allio/win32/iocp_multiplexer.hpp>

#include <allio/detail/assert.hpp>
#include <allio/detail/bitset.hpp>
#include <allio/implementation.hpp>
#include <allio/step_deadline.hpp>
#include <allio/win32/nt_error.hpp>
#include <allio/win32/platform.hpp>

#include <allio/impl/async_handle_types.hpp>

#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/platform_handle.hpp>

#include <array>
#include <span>

using namespace allio;
using namespace allio::win32;

static result<void> set_waitable_timer(HANDLE const handle, deadline const deadline)
{
	if (!SetWaitableTimer(
		handle,
		kernel_timeout(deadline),
		0,
		nullptr,
		nullptr,
		false))
	{
		return allio_ERROR(get_last_error());
	}
	return {};
}

static result<void> cancel_waitable_timer(HANDLE const handle)
{
	if (!CancelWaitableTimer(handle))
	{
		return allio_ERROR(get_last_error());
	}
	return {};
}

static result<bool> associate_wait_packet(
	HANDLE const wait_packet,
	HANDLE const completion_port,
	HANDLE const target_handle,
	PVOID const key_context,
	PVOID const apc_context,
	NTSTATUS const completion_status,
	ULONG_PTR const completion_information,
	bool const may_complete_synchronously)
{
	BOOLEAN already_signaled = false;
	NTSTATUS const status = NtAssociateWaitCompletionPacket(
		wait_packet,
		completion_port,
		target_handle,
		key_context,
		apc_context,
		completion_status,
		completion_information,
		may_complete_synchronously
			? &already_signaled
			: nullptr);

	if (!NT_SUCCESS(status))
	{
		return allio_ERROR(nt_error(status));
	}

	if (already_signaled)
	{
		// If the handle was already signaled, the wait packet must be cancelled
		// in order to remove the completion already queued on the completion port.
		// Verify because there is no good way to handle a spurious failure here.
		allio_VERIFY(NT_SUCCESS(NtCancelWaitCompletionPacket(wait_packet, true)));
	}

	return already_signaled;
}

static result<bool> cancel_wait_packet(HANDLE const handle, bool const remove_queued_completion)
{
	NTSTATUS const status = NtCancelWaitCompletionPacket(handle, remove_queued_completion);

	if (!NT_SUCCESS(status))
	{
		return allio_ERROR(nt_error(status));
	}

	return status != STATUS_PENDING;
}


allio_EXTERN_ASYNC_HANDLE_MULTIPLEXER_RELATIONS(iocp_multiplexer);

template <size_t Size>
HANDLE iocp_multiplexer::object_cache<Size>::try_acquire()
{
	if (auto const index = detail::bitset::flip_any(m_mask, true))
	{
		allio_ASSERT(!m_objects[*index]);
		return m_objects[*index].release();
	}
	return NULL;
}

template <size_t Size>
bool iocp_multiplexer::object_cache<Size>::try_release(HANDLE const handle)
{
	if (auto const index = detail::bitset::flip_any(m_mask, false))
	{
		allio_ASSERT(m_objects[*index]);
		m_objects[*index].reset(handle);
		return true;
	}
	return false;
}

result<iocp_multiplexer::init_result> iocp_multiplexer::init(init_options const& options)
{
	allio_TRYV(kernel_init());

	allio_TRY(completion_port, [&]() -> result<unique_handle>
	{
		HANDLE const completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);

		if (completion_port == NULL)
		{
			return allio_ERROR(get_last_error());
		}

		return result<unique_handle>(result_value, completion_port);
	}());

	result<init_result> r(result_value);
	r->completion_port = std::move(completion_port);
	return r;
}

iocp_multiplexer::iocp_multiplexer(init_result&& resources)
	: m_completion_port((
		allio_ASSERT(resources.completion_port),
		std::move(resources.completion_port)))
{
}

iocp_multiplexer::~iocp_multiplexer() = default;

type_id<multiplexer> iocp_multiplexer::get_type_id() const
{
	return type_of(*this);
}

result<multiplexer_handle_relation const*> iocp_multiplexer::find_handle_relation(type_id<handle> const handle_type) const
{
	return static_multiplexer_handle_relation_provider<iocp_multiplexer, async_handle_types>::find_handle_relation(handle_type);
}

result<multiplexer::statistics> iocp_multiplexer::pump(pump_parameters const& args)
{
	bool const complete = (args.mode & pump_mode::complete) != pump_mode::none;
	bool const flush = (args.mode & pump_mode::flush) != pump_mode::none;

	deadline deadline = args.deadline;
	return gather_statistics([&](statistics& statistics) -> result<void>
	{
		while (true)
		{
			if (flush)
			{
				deferring_multiplexer::flush();
			}

			if (!complete)
			{
				break;
			}

			FILE_IO_COMPLETION_INFORMATION entries[16];

			allio_TRY(entry_count, [&]() -> result<size_t>
			{
				using pair_type = std::pair<NTSTATUS, ULONG>;

				auto const [status, num_entries_removed] = [&]() -> pair_type
				{
					if (1) //TODO: use the multiple completion syscall if multiple operations have been submitted.
					{
						FILE_IO_COMPLETION_INFORMATION& entry = entries[0];

						return pair_type
						{
							NtRemoveIoCompletion(
								m_completion_port.get(),
								&entry.KeyContext,
								&entry.ApcContext,
								&entry.IoStatusBlock,
								kernel_timeout(deadline)),

							1,
						};
					}
					else
					{
						ULONG num_entries_removed;

						return pair_type
						{
							NtRemoveIoCompletionEx(
								m_completion_port.get(),
								entries,
								std::size(entries),
								&num_entries_removed,
								kernel_timeout(deadline),
								false),

							num_entries_removed,
						};
					}
				}();

				if (!NT_SUCCESS(status))
				{
					return allio_ERROR(static_cast<nt_error>(status));
				}

				if (status == STATUS_TIMEOUT)
				{
					return 0;
				}

				return num_entries_removed;
			}());

			if (entry_count == 0)
			{
				break;
			}

			for (auto const& entry : std::span(entries, entry_count))
			{
				io_slot* const slot = static_cast<io_status_block*>(
					reinterpret_cast<basic_io_slot_storage<IO_STATUS_BLOCK>*>(entries->ApcContext));

				if (void* const key_context = entry.KeyContext)
				{
					reinterpret_cast<io_completion_callback*>(key_context)(*this, *slot);
				}

				allio_ASSERT(slot->m_operation != nullptr);

				//TODO: Gather statistics.
				slot->m_operation->io_completed(*this, *slot);
			}

			//TODO: Resetting the timeout should only happen if some operation actually concluded.
			deadline = deadline::instant();
		}

		return {};
	});
}

static result<void> set_completion_information(HANDLE const handle, HANDLE const port, void* const key)
{
	IO_STATUS_BLOCK io_status_block = make_io_status_block();

	FILE_COMPLETION_INFORMATION file_completion_information =
	{
		.Port = port,
		.Key = key,
	};

	NTSTATUS status = NtSetInformationFile(
		handle,
		&io_status_block,
		&file_completion_information,
		sizeof(file_completion_information),
		port != nullptr
			? FileCompletionInformation
			: FileReplaceCompletionInformation);

	if (status == STATUS_PENDING)
	{
		status = io_wait(handle, &io_status_block, deadline());
	}

	if (!NT_SUCCESS(status))
	{
		return allio_ERROR(static_cast<nt_error>(status));
	}

	return {};
}

result<void> iocp_multiplexer::register_native_handle(native_platform_handle const handle)
{
	HANDLE const h = unwrap_handle(handle);
	allio_TRYV(set_completion_information(h, m_completion_port.get(), nullptr));
	return {};
}

result<void> iocp_multiplexer::deregister_native_handle(native_platform_handle const handle)
{
	HANDLE const h = unwrap_handle(handle);
	allio_TRYV(set_completion_information(h, NULL, nullptr));
	return {};
}

result<void> iocp_multiplexer::cancel_io(io_slot& slot, native_platform_handle const handle)
{
	//TODO: Does NtCancelIoFileEx behave as documented by CancelIoEx?
	IO_STATUS_BLOCK& io_status_block = *static_cast<iocp_multiplexer::io_status_block&>(slot);

	if (io_status_block.Status != STATUS_PENDING)
	{
		return {};
	}

	NTSTATUS const status = NtCancelIoFileEx(
		unwrap_handle(handle), &io_status_block, &io_status_block);

	if (!NT_SUCCESS(status) && status != STATUS_NOT_FOUND)
	{
		return allio_ERROR(nt_error(status));
	}

	return {};
}

result<bool> iocp_multiplexer::start_wait(wait_slot& slot, native_platform_handle const handle)
{
	wait_data& slot_data = *slot;

	allio_TRY(wait_packet, acquire_wait_packet());

	allio_TRY(already_signaled, associate_wait_packet(
		wait_packet.get(), m_completion_port.get(), unwrap_handle(handle),
		reinterpret_cast<PVOID>(handle_wait_completion), &slot_data, STATUS_SUCCESS, 0,
		/* may_complete_synchronously */ true));

	if (already_signaled)
	{
		slot_data.Status = STATUS_SUCCESS;
	}
	else
	{
		slot_data.Status = STATUS_PENDING;
		slot_data.wait_packet = wait_packet.release();
	}

	return already_signaled;
}

result<bool> iocp_multiplexer::cancel_wait(wait_slot& slot)
{
	wait_data& slot_data = *slot;

	if (slot_data.Status != STATUS_PENDING)
	{
		return false;
	}

	auto r = [&]() -> result<bool>
	{
		allio_TRY(cancelled, cancel_wait_packet(slot_data.wait_packet, false));

		if (cancelled)
		{
			slot_data.Status = STATUS_CANCELLED;
			release_wait_packet(slot_data.wait_packet);
		}

		return cancelled;
	}();

	if (!r)
	{
		// Something must have gone very wrong if an error occurred here.
		// We can at least attempt to release the system resources.

		if (slot_data.wait_packet != NULL)
		{
			(void)CloseHandle(slot_data.wait_packet);
		}

		//TODO: Set slot_data.Status?
	}

	return r;
}

void iocp_multiplexer::handle_wait_completion(iocp_multiplexer& multiplexer, io_slot& slot)
{
	wait_data& slot_data = *static_cast<wait_slot&>(slot);
	allio_ASSERT(slot_data.wait_packet != NULL);
	multiplexer.release_wait_packet(slot_data.wait_packet);
	slot_data.Status = STATUS_SUCCESS;
}

result<void> iocp_multiplexer::start_timeout(timeout_slot& slot, deadline const deadline)
{
	slot->Status = STATUS_PENDING;
	return {};
}

void iocp_multiplexer::cancel_timeout(timeout_slot& slot)
{
}

bool iocp_multiplexer::supports_synchronous_completion(platform_handle const& handle)
{
	return handle.get_flags()[platform_handle::implementation::flags::skip_completion_port_on_success];
}

result<iocp_multiplexer::unique_wait_packet> iocp_multiplexer::acquire_wait_packet()
{
	HANDLE handle = m_wait_packet_cache.try_acquire();

	if (handle == NULL)
	{
		NTSTATUS const status = NtCreateWaitCompletionPacket(&handle, 0, nullptr);

		if (!NT_SUCCESS(status))
		{
			return allio_ERROR(nt_error(status));
		}
	}

	return result<unique_wait_packet>(result_value, handle, *this);
}

void iocp_multiplexer::release_wait_packet(HANDLE const handle)
{
	if (!m_wait_packet_cache.try_release(handle))
	{
		allio_VERIFY(close_handle(handle));
	}
}

allio_TYPE_ID(iocp_multiplexer);
