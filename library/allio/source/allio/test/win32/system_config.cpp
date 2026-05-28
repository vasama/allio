#include <allio/test/system_config.hpp>

#include <allio/impl/syscall_hook.hpp>
#include <allio/impl/win32/kernel.hpp>

#include <unordered_map>

using namespace allio;
using namespace allio::test;

namespace {

struct iocp_config : syscall_hook<win32::NtSetInformationFile>
{
	NTSTATUS invoke(
		HANDLE const FileHandle,
		PIO_STATUS_BLOCK IoStatusBlock,
		PVOID const FileInformation,
		ULONG const Length,
		FILE_INFORMATION_CLASS const FileInformationClass) const override
	{
		if (FileInformationClass == FileReplaceCompletionInformation)
		{
			return STATUS_NOT_SUPPORTED;
		}

		if (FileInformationClass == FileIoCompletionNotificationInformation)
		{
			return STATUS_NOT_SUPPORTED;
		}

		return invoke_next(
			FileHandle,
			IoStatusBlock,
			FileInformation,
			Length,
			FileInformationClass);
	}
};


struct io_operation_state
{
	HANDLE notification_event;
	NTSTATUS completion_status;
};

static std::unordered_map<IO_STATUS_BLOCK*, io_operation_state> g_pending_operations;

} // namespace

system_config_ptr test::configure_default_multiplexer()
{
	return std::make_unique<iocp_config>();
}
