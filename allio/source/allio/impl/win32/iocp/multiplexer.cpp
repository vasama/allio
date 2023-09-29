#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>
#include <allio/win32/platform.hpp>

#include <vsm/lazy.hpp>

#include <span>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

namespace {

enum class key_context : uintptr_t
{
	generic,
	wait_packet,
};

} // namespace


static vsm::result<unique_handle> create_completion_port(size_t const max_concurrent_threads)
{
	HANDLE handle;
	NTSTATUS const status = NtCreateIoCompletion(
		&handle,
		IO_COMPLETION_ALL_ACCESS,
		/* ObjectAttributes: */ nullptr,
		max_concurrent_threads);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return vsm_lazy(unique_handle(handle));
}

vsm::result<iocp_multiplexer> iocp_multiplexer::_create(create_parameters const& args)
{
	vsm_try_void(kernel_init());

	if (args.max_concurrent_threads == 0)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	vsm_try(completion_port, create_completion_port(args.max_concurrent_threads));

	vsm::intrusive_ptr<shared_state_t> shared_state(new shared_state_t);
	shared_state->completion_port = vsm_move(completion_port);

	return vsm_lazy(iocp_multiplexer(vsm_move(shared_state)));
}

vsm::result<iocp_multiplexer> iocp_multiplexer::_create(iocp_multiplexer const& other)
{
	if (!other.m_shared_state)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	return vsm_lazy(iocp_multiplexer(other.m_shared_state));
}

iocp_multiplexer::iocp_multiplexer(vsm::intrusive_ptr<shared_state_t> shared_state)
	: m_shared_state(vsm_move(shared_state))
	, m_completion_port(m_shared_state->completion_port.get())
{
}


static vsm::result<void> set_completion_information(
	HANDLE const handle,
	HANDLE const completion_port,
	void* const key_context)
{
	IO_STATUS_BLOCK io_status_block = make_io_status_block();

	FILE_COMPLETION_INFORMATION file_completion_information =
	{
		.Port = completion_port,
		.Key = key_context,
	};

	NTSTATUS const status = NtSetInformationFile(
		handle,
		&io_status_block,
		&file_completion_information,
		sizeof(file_completion_information),
		completion_port != nullptr
			? FileCompletionInformation
			: FileReplaceCompletionInformation);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

vsm::result<void> iocp_multiplexer::attach(platform_handle const& h, connector_type& c)
{ 
	return set_completion_information(
		unwrap_handle(h.get_platform_handle()),
		m_completion_port.value,
		std::bit_cast<void*>(key_context::generic));
}

void iocp_multiplexer::detach(platform_handle const& h, connector_type& c, error_handler* const e)
{
	auto const r = set_completion_information(
		unwrap_handle(h.get_platform_handle()),
		NULL,
		nullptr);

	if (!r)
	{
		get_error_handler(e).handle_error(
		{
			.error = r.error(),
			.source = error_source{},
		});
	}
}


bool iocp_multiplexer::cancel_io(io_slot& slot, native_platform_handle const handle)
{
	size_t const io_status_block_offset = io_status_block::get_storage_offset();
	static_assert(io_status_block_offset == overlapped::get_storage_offset());

	void* const isb = reinterpret_cast<void*>(
		reinterpret_cast<uintptr_t>(&slot) + io_status_block_offset);

#if 0
	// This copy may cause a data race if the kernel writes the ISB.
	//TODO: Figure out if this is worthwhile, and if so, do it atomically.
	IO_STATUS_BLOCK isb_copy;
	memcpy(&isb_copy, isb, sizeof(IO_STATUS_BLOCK));

	if (isb_copy.Status != STATUS_PENDING)
	{
		return {};
	}
#endif

	NTSTATUS const status = NtCancelIoFileEx(
		unwrap_handle(handle),
		reinterpret_cast<IO_STATUS_BLOCK*>(isb),
		reinterpret_cast<IO_STATUS_BLOCK*>(isb));

	if (NT_SUCCESS(status))
	{
		return true;
	}

	if (status != STATUS_NOT_FOUND)
	{
		unrecoverable_error(static_cast<kernel_error>(status));
	}

	return false;
}


static constexpr ACCESS_MASK wait_packet_all_access = 1;

static vsm::result<unique_wait_packet> create_wait_packet()
{
	HANDLE packet;
	NTSTATUS const status = NtCreateWaitCompletionPacket(
		&packet,
		wait_packet_all_access,
		/* ObjectAttributes: */ nullptr);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return vsm::result<unique_wait_packet>(vsm::result_value, wrap_wait_packet(packet));
}

static vsm::result<bool> associate_wait_packet(
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
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	if (already_signaled)
	{
		// If the handle was already signaled, the wait packet must be cancelled
		// in order to remove the completion already queued on the completion port.
		//TODO: Check if it's possible to skip completion port on wait packet synchronous completion.
		//      If not, document that here.
		NTSTATUS const cancel_status = NtCancelWaitCompletionPacket(
			wait_packet,
			/* RemoveSignaledPacket: */ true);
		
		if (!NT_SUCCESS(cancel_status))
		{
			// There is no good way to handle cancellation failure.
			unrecoverable_error(static_cast<kernel_error>(cancel_status));
		}
	}

	return already_signaled;
}

static vsm::result<bool> cancel_wait_packet(
	HANDLE const handle,
	bool const remove_queued_completion)
{
	NTSTATUS const status = NtCancelWaitCompletionPacket(
		handle,
		remove_queued_completion);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return status != STATUS_PENDING;
}

vsm::result<unique_wait_packet> iocp_multiplexer::acquire_wait_packet()
{
	return create_wait_packet();
}

void iocp_multiplexer::release_wait_packet(unique_wait_packet&& packet)
{
	packet.reset();
}

vsm::result<bool> iocp_multiplexer::submit_wait(
	wait_packet const packet,
	wait_slot& slot,
	native_platform_handle const handle)
{
	vsm_assert(slot.m_operation != nullptr);

	return associate_wait_packet(
		unwrap_wait_packet(packet),
		m_completion_port.value,
		unwrap_handle(handle),
		std::bit_cast<void*>(key_context::wait_packet),
		&slot,
		STATUS_SUCCESS,
		/* completion_information: */ 0,
		/* may_complete_synchronously: */ true);
}

bool iocp_multiplexer::cancel_wait(wait_packet const packet)
{
	return unrecoverable(
		cancel_wait_packet(
			unwrap_wait_packet(packet),
			/* remove_queued_completion: */ false),
		/* default_value: */ false);
}


static vsm::result<size_t> remove_io_completions(
	HANDLE const completion_port,
	std::span<FILE_IO_COMPLETION_INFORMATION> const buffer,
	deadline const deadline)
{
	NTSTATUS status;
	ULONG num_entries_removed;

	if (buffer.size() == 1)
	{
		FILE_IO_COMPLETION_INFORMATION& entry = buffer.front();
		
		status = NtRemoveIoCompletion(
			completion_port,
			&entry.KeyContext,
			&entry.ApcContext,
			&entry.IoStatusBlock,
			kernel_timeout(deadline));
			
		num_entries_removed = 1;
	}
	else
	{
		status = NtRemoveIoCompletionEx(
			completion_port,
			buffer.data(),
			buffer.size(),
			&num_entries_removed,
			kernel_timeout(deadline),
			/* Alertable: */ false);
	}

	if (status == STATUS_TIMEOUT)
	{
		return 0;
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}
	
	return num_entries_removed;
}

vsm::result<bool> iocp_multiplexer::_poll(poll_parameters const& args)
{
	bool made_progress = false;

	poll_mode const mode = args.mode;
	deadline deadline = args.deadline;

#if 0
	if (vsm::any_flags(mode, poll_mode::flush))
	{
		if (flush(local_poll_statistics(args.statistics)))
		{
			made_progress = true;
			deadline = deadline::instant();
		}
	}
#endif

	if (vsm::any_flags(mode, poll_mode::complete))
	{
		FILE_IO_COMPLETION_INFORMATION entries[16];

		//TODO: Pass more than one depending on how many operations have been submitted.
		vsm_try(entry_count, remove_io_completions(
			m_completion_port.value,
			std::span(entries, 1),
			deadline));
		
		if (entry_count != 0)
		{
			made_progress = true;
			for (auto const& entry : std::span(entries, entry_count))
			{
				switch (std::bit_cast<key_context>(entry.KeyContext))
				{
				case key_context::generic:
					{
						size_t const io_status_block_offset = io_status_block::get_storage_offset();
						static_assert(io_status_block_offset == overlapped::get_storage_offset());

						io_slot& slot = *std::launder(reinterpret_cast<io_slot*>(
							reinterpret_cast<uintptr_t>(entry.ApcContext) - io_status_block_offset));

						vsm_assert(slot.m_operation != nullptr);
						operation_type& operation = *slot.m_operation;

						operation.notify(wrap_io_status(slot));
					}
					break;

				case key_context::wait_packet:
					{
						wait_slot& slot = *static_cast<wait_slot*>(entry.ApcContext);

						vsm_assert(slot.m_operation != nullptr);
						operation_type& operation = *slot.m_operation;

						wait_status status =
						{
							.slot = slot,
							.status = entry.IoStatusBlock.Status,
						};
						operation.notify(wrap_io_status(status));
					}
					break;
				}

			}
		}
	}

	return made_progress;
}
