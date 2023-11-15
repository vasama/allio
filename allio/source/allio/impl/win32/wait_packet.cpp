#include <allio/impl/win32/wait_packet.hpp>

#include <allio/win32/kernel_error.hpp>

using namespace allio;
using namespace allio::win32;

static constexpr ACCESS_MASK wait_packet_all_access = 1;

vsm::result<unique_wait_packet> win32::create_wait_packet()
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

vsm::result<bool> win32::associate_wait_packet(
	HANDLE const wait_packet,
	HANDLE const completion_port,
	HANDLE const target_handle,
	PVOID const key_context,
	PVOID const apc_context,
	NTSTATUS const completion_status,
	ULONG_PTR const completion_information)
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
		&already_signaled);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return already_signaled;
}

vsm::result<bool> win32::cancel_wait_packet(
	HANDLE const wait_packet,
	bool const remove_queued_completion)
{
	NTSTATUS const status = NtCancelWaitCompletionPacket(
		wait_packet,
		remove_queued_completion);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return status != STATUS_PENDING;
}
