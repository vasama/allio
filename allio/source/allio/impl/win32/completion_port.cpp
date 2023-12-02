#include <allio/impl/win32/completion_port.hpp>

#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>

#include <vsm/lazy.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<unique_handle> win32::create_completion_port(size_t const max_concurrent_threads)
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

vsm::result<void> win32::set_completion_information(
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

vsm::result<void> win32::set_io_completion(
	HANDLE const completion_port,
	void* const key_context,
	void* const apc_context,
	NTSTATUS const completion_status,
	ULONG const completion_information)
{
	NTSTATUS const status = NtSetIoCompletion(
		completion_port,
		key_context,
		apc_context,
		completion_status,
		completion_information);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

vsm::result<size_t> win32::remove_io_completions(
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
