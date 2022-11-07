#include <allio/detail/preprocessor.h>

#include <allio/impl/win32/kernel.hpp>

#include <atomic>

using namespace allio;
using namespace allio::win32;

#define allio_WINDOWS_SYSCALLS(X, ...) \
	X(RtlNtStatusToDosError,                    ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(NtWaitForSingleObject,                    ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(NtSetInformationFile,                     ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(NtCreateFile,                             ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(NtReadFile,                               ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(NtWriteFile,                              ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(NtCancelIoFileEx,                         ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(NtRemoveIoCompletion,                     ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(NtRemoveIoCompletionEx,                   ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(NtCreateWaitCompletionPacket,             ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(NtAssociateWaitCompletionPacket,          ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(NtCancelWaitCompletionPacket,             ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(NtQueryDirectoryFileEx,                   ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(NtQueryFullAttributesFile,                ntdll       __VA_OPT__(, __VA_ARGS__)) \

#define allio_X(syscall, ...) \
	decltype(win32::syscall) win32::syscall = nullptr;

allio_WINDOWS_SYSCALLS(allio_X)
#undef allio_X

result<void> win32::kernel_init()
{
	static result<void> const r = []() -> result<void>
	{
		HMODULE const ntdll = GetModuleHandleA("NTDLL.DLL");

#define allio_X(syscall, dll) \
		if (FARPROC const proc = GetProcAddress(dll, allio_detail_STR(syscall))) \
		{ \
			win32::syscall = reinterpret_cast<decltype(win32::syscall)>(proc); \
		} \
		else \
		{ \
			goto return_error; \
		} \

		allio_WINDOWS_SYSCALLS(allio_X)
#undef allio_X

		return {};

	return_error:
		return allio_ERROR(error::unsupported_operation);
	}();
	return r;
}





NTSTATUS win32::io_wait(HANDLE const handle, IO_STATUS_BLOCK* const isb, deadline const deadline)
{
	if (isb->Status == STATUS_PENDING)
	{
		do
		{
			//TODO: deadline
			NTSTATUS status = NtWaitForSingleObject(handle, 0, nullptr);

			if (status == STATUS_TIMEOUT)
			{
				status = io_cancel(handle, isb);

				if (status < 0 && status != STATUS_CANCELLED)
				{
					abort();
				}

				return isb->Status = STATUS_TIMEOUT;
			}
		}
		while (isb->Status == STATUS_PENDING);
	}
	else
	{
		allio_ASSERT(isb->Status != -1);
	}
	return isb->Status;
}

NTSTATUS win32::io_cancel(HANDLE const handle, IO_STATUS_BLOCK* const isb)
{
	if (isb->Status != STATUS_PENDING)
	{
		return isb->Status;
	}

	NTSTATUS status = NtCancelIoFileEx(handle, isb, isb);

	if (status < 0)
	{
		if (status == STATUS_NOT_FOUND)
		{
			//TODO: why consider it cancelled here?
			return isb->Status = STATUS_CANCELLED;
		}

		return status;
	}

	if (isb->Status == 0)
	{
		//TODO: why consider it cancelled here?
		return isb->Status = STATUS_CANCELLED;
	}

	return isb->Status;
}
