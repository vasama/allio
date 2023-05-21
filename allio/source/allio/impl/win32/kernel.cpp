#include <allio/impl/win32/kernel.hpp>

#include <allio/error.hpp>

#include <vsm/preprocessor.h>

#include <atomic>

using namespace allio;
using namespace allio::win32;

#define allio_nt_syscalls(X, ...) \
	X(RtlNtStatusToDosError,                    ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(RtlSetCurrentDirectory_U,                 ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(NtWaitForSingleObject,                    ntdll       __VA_OPT__(, __VA_ARGS__)) \
	X(NtQueryInformationFile,                   ntdll       __VA_OPT__(, __VA_ARGS__)) \
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

#define allio_x_entry(syscall, ...) \
	decltype(win32::syscall) win32::syscall = nullptr;

allio_nt_syscalls(allio_x_entry)
#undef allio_x_entry

vsm::result<void> win32::kernel_init()
{
	static vsm::result<void> const r = []() -> vsm::result<void>
	{
		HMODULE const ntdll = GetModuleHandleA("NTDLL.DLL");

#define allio_x_entry(syscall, dll) \
		if (FARPROC const proc = GetProcAddress(dll, vsm_pp_str(syscall))) \
		{ \
			win32::syscall = reinterpret_cast<decltype(win32::syscall)>(proc); \
		} \
		else \
		{ \
			goto return_error; \
		} \

		allio_nt_syscalls(allio_x_entry)
#undef allio_x_entry

		return {};

	return_error:
		return std::unexpected(error::unsupported_operation);
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
		vsm_assert(isb->Status != -1);
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
