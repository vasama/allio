#include <allio/win32/iocp/multiplexer.hpp>

#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>
#include <allio/win32/platform.hpp>
//#include <allio/win32/handles/platform_handle.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using handle_type = iocp_multiplexer::handle_type;


static vsm::result<void> set_completion_information(HANDLE const handle, HANDLE const port)
{
	IO_STATUS_BLOCK io_status_block = make_io_status_block();

	FILE_COMPLETION_INFORMATION file_completion_information =
	{
		.Port = port,
		.Key = nullptr,
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
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

vsm::result<void> handle_type::attach(iocp_multiplexer& m, platform_handle const& h)
{
	return set_completion_information(unwrap_handle(h.get_platform_handle()), m.m_completion_port.get());
}

vsm::result<void> handle_type::detach(iocp_multiplexer& m, platform_handle const& h)
{
	return set_completion_information(unwrap_handle(h.get_platform_handle()), NULL);
}


vsm::result<bool> handle_type::cancel_io(iocp_multiplexer& m, io_slot& slot, native_platform_handle const handle) const
{
	//TODO: This is a undefined behaviour...
	IO_STATUS_BLOCK& io_status_block = *static_cast<iocp_multiplexer::io_status_block&>(slot);

	if (io_status_block.Status != STATUS_PENDING)
	{
		return {};
	}
	
	//TODO: Does NtCancelIoFileEx behave as documented by CancelIoEx?
	NTSTATUS const status = NtCancelIoFileEx(
		unwrap_handle(handle),
		&io_status_block,
		&io_status_block);
	
	if (status == STATUS_NOT_FOUND)
	{
		return false;
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return true;
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
			unrecoverable(static_cast<kernel_error>(cancel_status));
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

//TODO: Make these static? They don't actually need access to the handle context.
vsm::result<void> handle_type::submit_wait(iocp_multiplexer& m, io_slot& slot, unique_wait_packet& packet, native_platform_handle const handle) const
{
	if (!packet)
	{
		vsm_try_assign(packet, m.acquire_wait_packet());
	}

	vsm_try(already_signaled, associate_wait_packet(
		packet.get(),
		m.m_completion_port.get(),
		unwrap_handle(handle),
		/* key_context: */ nullptr,
		&slot,
		STATUS_SUCCESS,
		/* completion_information: */ 0,
		/* may_complete_synchronously: */ true))

	if (already_signaled)
	{
		slot_data.Status = STATUS_SUCCESS;
	}
	else
	{
		slot_data.Status = STATUS_PENDING;
	}

	return already_signaled;
}

vsm::result<bool> handle_type::cancel_wait(iocp_multipleser& m, io_slot& slot, unique_wait_packet& packet, native_platform_handle const handle) const
{
	vsm_try(cancelled, cancel_wait_packet(
		packet.get(),
		/* remove_queued_completion: */ false));

	if (cancelled)
	{
		m.release_wait_packet(packet);
	}
	
	return cancelled;
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
			entries.data(),
			entries.size(),
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

vsm::result<bool> iocp_multiplexer::poll(poll_parameters const& args)
{
	poll_mode const mode = args.mode;
	deadline deadline = args.deadline;

	bool made_progress = false;

	if (vsm::any_flags(mode, poll_mode::flush))
	{
		if (flush())
		{
			made_progress = true;
			deadline = deadline::instant();
		}
	}

	if (vsm::any_flags(mode, poll_mode::complete))
	{
		FILE_IO_COMPLETION_INFORMATION entries[16];

		//TODO: Pass more than one depending on how many operations have been submitted.
		vsm_try(entry_count, remove_io_completions(
			m_completion_port.get(),
			std::span(entries, 1),
			deadline));
		
		if (entry_count == 0)
		{
			break;
		}

		made_progress = true;
		for (auto const& entry : std::span(entries, entry_count))
		{
			//TODO: Think about this. It's a bit sus.
			io_slot* const = static_cast<io_status_block*>(
				reinterpret_cast<basic_io_slot_storage<IO_STATUS_BLOCK>*>(entry.ApcContext));

			vsm_assert(slot->m_operation != nullptr);
			slot->m_operation->m_io_handler(*this, *slot->m_operation, *slot);
		}
	}

	return made_progress;
}
