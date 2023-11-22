#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/impl/win32/completion_port.hpp>
#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/wait_packet.hpp>
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


vsm::result<iocp_multiplexer> iocp_multiplexer::_create(create_parameters const& args)
{
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


vsm::result<void> iocp_multiplexer::attach_handle(native_platform_handle const handle, connector_type&)
{
	return set_completion_information(
		unwrap_handle(handle),
		m_completion_port.value,
		std::bit_cast<void*>(key_context::generic));
}

vsm::result<void> iocp_multiplexer::detach_handle(native_platform_handle const handle, connector_type&)
{
	return set_completion_information(
		unwrap_handle(handle),
		NULL,
		nullptr);
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
	vsm_assert(slot.m_handler != nullptr);

	vsm_try(already_signaled, associate_wait_packet(
		unwrap_wait_packet(packet),
		m_completion_port.value,
		unwrap_handle(handle),
		std::bit_cast<void*>(key_context::wait_packet),
		/* apc_context: */ &slot,
		STATUS_SUCCESS,
		/* completion_information: */ 0));

	if (already_signaled)
	{
		//TODO: We cannot simply remove the completion if another thread has concurrently
		//      removed the completion and reused the wait packet. It can only be removed
		//      if it is guaranteed that this multiplexer is not being used concurrently.
		already_signaled = false;
	}

	return already_signaled;
}

bool iocp_multiplexer::cancel_wait(wait_packet const packet, wait_slot& slot)
{
	auto const r = cancel_wait_packet(
		unwrap_wait_packet(packet),
		/* remove_queued_completion: */ false);

	if (!r)
	{
		unrecoverable_error(r.error());
		return false;
	}

	if (*r)
	{
		// If the wait was cancelled before its completion,
		// a completion event must be queued manually.
		//TODO: Instead of going through the OS,
		//      it might be better to queue this in user space.
		vsm_verify(set_io_completion(
			m_completion_port.value,
			std::bit_cast<void*>(key_context::wait_packet),
			/* apc_context: */ &slot,
			STATUS_CANCELLED,
			/* completion_information: */ 0));
	}

	return true;
}


vsm::result<bool> iocp_multiplexer::_poll(poll_parameters const& args)
{
	FILE_IO_COMPLETION_INFORMATION entries[32];

	vsm_try(entry_count, remove_io_completions(
		m_completion_port.value,
		entries,
		args.deadline));

	if (entry_count == 0)
	{
		return false;
	}

	for (auto const& entry : std::span(entries, entry_count))
	{
		switch (std::bit_cast<key_context>(entry.KeyContext))
		{
		case key_context::generic:
			{
				size_t const io_status_block_offset = io_status_block::get_storage_offset();
				static_assert(io_status_block_offset == overlapped::get_storage_offset());

				// The ApcContext contains the pointer to the original IO_STATUS_BLOCK
				// or OVERLAPPED passed to the I/O API when this operation was started.
				io_slot& slot = *std::launder(reinterpret_cast<io_slot*>(
					reinterpret_cast<uintptr_t>(entry.ApcContext) - io_status_block_offset));

				vsm_assert(slot.m_handler != nullptr);
				slot.m_handler->notify(io_status_type
				{
					.slot = slot,
					.status = entry.IoStatusBlock.Status,
				});
			}
			break;

		case key_context::wait_packet:
			{
				wait_slot& slot = *static_cast<wait_slot*>(entry.ApcContext);

				vsm_assert(slot.m_handler != nullptr);
				slot.m_handler->notify(io_status_type
				{
					.slot = slot,
					.status = entry.IoStatusBlock.Status,
				});
			}
			break;
		}

	}

	return true;
}
