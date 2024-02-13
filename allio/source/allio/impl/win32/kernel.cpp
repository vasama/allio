#include <allio/impl/win32/kernel.hpp>

#include <allio/error.hpp>

#include <vsm/preprocessor.h>

#include <atomic>

using namespace allio;
using namespace allio::win32;

#define allio_kernel_modules(X) \
	X(ntdll) \

#define allio_kernel_functions(X) \
	X(ntdll,        RtlNtStatusToDosError,                  ERROR_NOT_SUPPORTED) \
	X(ntdll,        RtlSetCurrentDirectory_U,               STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtClose,                                STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtWaitForSingleObject,                  STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtQueryInformationFile,                 STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtSetInformationFile,                   STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtCreateFile,                           STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtReadFile,                             STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtWriteFile,                            STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtCancelIoFileEx,                       STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtCreateIoCompletion,                   STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtSetIoCompletion,                      STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtRemoveIoCompletion,                   STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtRemoveIoCompletionEx,                 STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtCreateWaitCompletionPacket,           STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtAssociateWaitCompletionPacket,        STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtCancelWaitCompletionPacket,           STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtQueryDirectoryFileEx,                 STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtQueryFullAttributesFile,              STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtAllocateVirtualMemory,                STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtFreeVirtualMemory,                    STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtProtectVirtualMemory,                 STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtCreateSection,                        STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtMapViewOfSection,                     STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtUnmapViewOfSection,                   STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtCreateEvent,                          STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtSetEvent,                             STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtResetEvent,                           STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtDeviceIoControlFile,                  STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtQueryObject,                          STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtTerminateProcess,                     STATUS_NOT_SUPPORTED) \
	X(ntdll,        NtCreateNamedPipeFile,                  STATUS_NOT_SUPPORTED) \

namespace {

template<typename Pointer>
struct kernel_function;

template<typename R, typename... P>
struct kernel_function<R(NTAPI*)(P...)>
{
	using Pointer = R(NTAPI*)(P...);

	template<
		HMODULE& Module,
		wchar_t const* ModuleName,
		Pointer& Function,
		char const* FunctionName,
		R DefaultValue>
	static R NTAPI entry_point(P const... args)
	{
		auto _module = Module;

		if (_module == NULL)
		{
			_module = GetModuleHandleW(ModuleName);

			if (_module == NULL)
			{
				Function = return_default<DefaultValue>;
				return DefaultValue;
			}

			Module = _module;
		}

		auto const function = reinterpret_cast<Pointer>(
			GetProcAddress(_module, FunctionName));

		if (function == NULL)
		{
			Function = return_default<DefaultValue>;
			return DefaultValue;
		}

		Function = function;
		return function(args...);
	}

	template<R DefaultValue>
	static R NTAPI return_default(P...)
	{
		return DefaultValue;
	}
};

namespace module_names {

#define allio_x_entry(module) \
	static constexpr wchar_t module[] = L"" vsm_pp_str(module) ".dll";

allio_kernel_modules(allio_x_entry)
#undef allio_x_entry

} // namespace module_names

namespace modules {

#define allio_x_entry(module) \
	static HMODULE module = NULL;

allio_kernel_modules(allio_x_entry)
#undef allio_x_entry

} // namespace modules

namespace function_names {

#define allio_x_entry(module, function, ...) \
	static constexpr char function[] = "" vsm_pp_str(function);

allio_kernel_functions(allio_x_entry)
#undef allio_x_entry

} // namespace function_names

} // namespace

#define allio_x_entry(module, function, default_value) \
	decltype(win32::function) win32::function = \
		kernel_function<decltype(win32::function)>::entry_point< \
			modules::module, \
			module_names::module, \
			win32::function, \
			function_names::function, \
			default_value>; \

allio_kernel_functions(allio_x_entry)
#undef allio_x_entry


NTSTATUS win32::io_wait(HANDLE const handle, IO_STATUS_BLOCK* const isb, deadline)
{
	if (isb->Status == STATUS_PENDING)
	{
		do
		{
			//TODO: deadline
			NTSTATUS status = NtWaitForSingleObject(
				handle,
				/* Alertable: */ FALSE,
				/* Timeout: */ nullptr);

			if (status == STATUS_TIMEOUT)
			{
				status = io_cancel(handle, *isb);

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

NTSTATUS win32::io_cancel(HANDLE const handle, IO_STATUS_BLOCK& io_status_block)
{
	if (io_status_block.Status != STATUS_PENDING)
	{
		return io_status_block.Status;
	}

	NTSTATUS status = NtCancelIoFileEx(
		handle,
		&io_status_block,
		&io_status_block);

	if (status < 0)
	{
		if (status == STATUS_NOT_FOUND)
		{
			//TODO: why consider it cancelled here?
			return io_status_block.Status = STATUS_CANCELLED;
		}

		return status;
	}

	if (io_status_block.Status == 0)
	{
		//TODO: why consider it cancelled here?
		return io_status_block.Status = STATUS_CANCELLED;
	}

	return io_status_block.Status;
}
