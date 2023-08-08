#pragma once

#include <allio/deadline.hpp>

#include <vsm/assert.h>
#include <vsm/result.hpp>

#include <win32.h>
#include <winternl.h>

namespace allio::win32 {

inline constexpr NTSTATUS STATUS_SUCCESS                        = 0;
inline constexpr NTSTATUS STATUS_NO_MORE_FILES                  = 0x80000006;
inline constexpr NTSTATUS STATUS_BUFFER_TOO_SMALL               = 0xC0000023;
inline constexpr NTSTATUS STATUS_OBJECT_NAME_INVALID            = 0xC0000033;
inline constexpr NTSTATUS STATUS_CANCELLED                      = 0xC0000120;
inline constexpr NTSTATUS STATUS_NOT_FOUND                      = 0xC0000225;


inline constexpr ULONG SL_RESTART_SCAN                          = 0x00000001;


typedef void(NTAPI* PIO_APC_ROUTINE)(
	_In_ PVOID ApcContext,
	_In_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_ ULONG Reserved);

enum _FILE_INFORMATION_CLASS
{
	FileDirectoryInformation = 1,
	FileFullDirectoryInformation,
	FileBothDirectoryInformation,
	FileBasicInformation,
	FileStandardInformation,
	FileInternalInformation,
	FileEaInformation,
	FileAccessInformation,
	FileNameInformation,
	FileRenameInformation,
	FileLinkInformation,
	FileNamesInformation,
	FileDispositionInformation,
	FilePositionInformation,
	FileFullEaInformation,
	FileModeInformation,
	FileAlignmentInformation,
	FileAllInformation,
	FileAllocationInformation,
	FileEndOfFileInformation,
	FileAlternateNameInformation,
	FileStreamInformation,
	FilePipeInformation,
	FilePipeLocalInformation,
	FilePipeRemoteInformation,
	FileMailslotQueryInformation,
	FileMailslotSetInformation,
	FileCompressionInformation,
	FileObjectIdInformation,
	FileCompletionInformation,
	FileMoveClusterInformation,
	FileQuotaInformation,
	FileReparsePointInformation,
	FileNetworkOpenInformation,
	FileAttributeTagInformation,
	FileTrackingInformation,
	FileIdBothDirectoryInformation,
	FileIdFullDirectoryInformation,
	FileValidDataLengthInformation,
	FileShortNameInformation,
	FileIoCompletionNotificationInformation,
	FileIoStatusBlockRangeInformation,
	FileIoPriorityHintInformation,
	FileSfioReserveInformation,
	FileSfioVolumeInformation,
	FileHardLinkInformation,
	FileProcessIdsUsingFileInformation,
	FileNormalizedNameInformation,
	FileNetworkPhysicalNameInformation,
	FileIdGlobalTxDirectoryInformation,
	FileIsRemoteDeviceInformation,
	FileUnusedInformation,
	FileNumaNodeInformation,
	FileStandardLinkInformation,
	FileRemoteProtocolInformation,
	FileRenameInformationBypassAccessCheck,
	FileLinkInformationBypassAccessCheck,
	FileVolumeNameInformation,
	FileIdInformation,
	FileIdExtdDirectoryInformation,
	FileReplaceCompletionInformation,
	FileHardLinkFullIdInformation,
	FileIdExtdBothDirectoryInformation,
	FileDispositionInformationEx,
	FileRenameInformationEx,
	FileRenameInformationExBypassAccessCheck,
	FileDesiredStorageClassInformation,
	FileStatInformation,
	FileMemoryPartitionInformation,
	FileStatLxInformation,
	FileCaseSensitiveInformation,
	FileLinkInformationEx,
	FileLinkInformationExBypassAccessCheck,
	FileStorageReserveIdInformation,
	FileCaseSensitiveInformationForceAccessCheck,
	FileMaximumInformation
}
typedef FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

struct _FILE_NAME_INFORMATION
{
	ULONG FileNameLength;
	WCHAR FileName[1];
}
typedef FILE_NAME_INFORMATION, *PFILE_NAME_INFORMATION;

struct _FILE_COMPLETION_INFORMATION
{
	HANDLE Port;
	PVOID Key;
}
typedef FILE_COMPLETION_INFORMATION, *PFILE_COMPLETION_INFORMATION;

struct _FILE_IO_COMPLETION_INFORMATION
{
	PVOID KeyContext;
	PVOID ApcContext;
	IO_STATUS_BLOCK IoStatusBlock;
}
typedef FILE_IO_COMPLETION_INFORMATION, *PFILE_IO_COMPLETION_INFORMATION;

struct _FILE_NETWORK_OPEN_INFORMATION
{
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER AllocationSize;
	LARGE_INTEGER EndOfFile;
	ULONG FileAttributes;
	ULONG Reserved;
}
typedef FILE_NETWORK_OPEN_INFORMATION, *PFILE_NETWORK_OPEN_INFORMATION;

struct _FILE_ID_FULL_DIR_INFORMATION
{
	ULONG NextEntryOffset;
	ULONG FileIndex;
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER EndOfFile;
	LARGE_INTEGER AllocationSize;
	ULONG FileAttributes;
	ULONG FileNameLength;
	ULONG EaSize;
	ULONG ReparsePointTag;
	LARGE_INTEGER FileId;
	WCHAR FileName[1];
}
typedef FILE_ID_FULL_DIR_INFORMATION, *PFILE_ID_FULL_DIR_INFORMATION;

struct _FILE_ID_EXTD_DIR_INFORMATION
{
	ULONG NextEntryOffset;
	ULONG FileIndex;
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER EndOfFile;
	LARGE_INTEGER AllocationSize;
	ULONG FileAttributes;
	ULONG FileNameLength;
	ULONG EaSize;
	ULONG ReparsePointTag;
	FILE_ID_128 FileId;
	WCHAR FileName[1];
}
typedef FILE_ID_EXTD_DIR_INFORMATION, *PFILE_ID_EXTD_DIR_INFORMATION;

struct _RTL_RELATIVE_NAME
{
	UNICODE_STRING RelativeName;
	HANDLE ContainingDirectory;
	void* CurDirRef;
}
typedef RTL_RELATIVE_NAME, *PRTL_RELATIVE_NAME;

struct _RTL_DRIVE_LETTER_CURDIR
{
	USHORT Flags;
	USHORT Length;
	ULONG TimeStamp;
	UNICODE_STRING DosPath;
}
typedef RTL_DRIVE_LETTER_CURDIR, *PRTL_DRIVE_LETTER_CURDIR;

struct _RTL_USER_PROCESS_PARAMETERS
{
	ULONG MaximumLength;
	ULONG Length;
	ULONG Flags;
	ULONG DebugFlags;
	PVOID ConsoleHandle;
	ULONG ConsoleFlags;
	HANDLE StdInputHandle;
	HANDLE StdOutputHandle;
	HANDLE StdErrorHandle;
	UNICODE_STRING CurrentDirectoryPath;
	HANDLE CurrentDirectoryHandle;
	UNICODE_STRING DllPath;
	UNICODE_STRING ImagePathName;
	UNICODE_STRING CommandLine;
	PVOID Environment;
	ULONG StartingPositionLeft;
	ULONG StartingPositionTop;
	ULONG Width;
	ULONG Height;
	ULONG CharWidth;
	ULONG CharHeight;
	ULONG ConsoleTextAttributes;
	ULONG WindowFlags;
	ULONG ShowWindowFlags;
	UNICODE_STRING WindowTitle;
	UNICODE_STRING DesktopName;
	UNICODE_STRING ShellInfo;
	UNICODE_STRING RuntimeData;
	RTL_DRIVE_LETTER_CURDIR DLCurrentDirectory[0x20];
}
typedef RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;

struct _PEB
{
	BOOLEAN InheritedAddressSpace;
	BOOLEAN ReadImageFileExecOptions;
	BOOLEAN BeingDebugged;
	BOOLEAN Spare;
	HANDLE Mutant;
	PVOID ImageBaseAddress;
	PPEB_LDR_DATA LoaderData;
	PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
	PVOID SubSystemData;
	PVOID ProcessHeap;
	PRTL_CRITICAL_SECTION FastPebLock;
	PVOID FastPebLockRoutine;
	PVOID FastPebUnlockRoutine;
	ULONG EnvironmentUpdateCount;
	PVOID* KernelCallbackTable;
	PVOID EventLogSection;
	PVOID EventLog;
	PVOID FreeList;
	ULONG TlsExpansionCounter;
	PVOID TlsBitmap;
	ULONG TlsBitmapBits[0x2];
	PVOID ReadOnlySharedMemoryBase;
	PVOID ReadOnlySharedMemoryHeap;
	PVOID* ReadOnlyStaticServerData;
	PVOID AnsiCodePageData;
	PVOID OemCodePageData;
	PVOID UnicodeCaseTableData;
	ULONG NumberOfProcessors;
	ULONG NtGlobalFlag;
	BYTE Spare2[0x4];
	LARGE_INTEGER CriticalSectionTimeout;
	ULONG HeapSegmentReserve;
	ULONG HeapSegmentCommit;
	ULONG HeapDeCommitTotalFreeThreshold;
	ULONG HeapDeCommitFreeBlockThreshold;
	ULONG NumberOfHeaps;
	ULONG MaximumNumberOfHeaps;
	PVOID** ProcessHeaps;
	PVOID GdiSharedHandleTable;
	PVOID ProcessStarterHelper;
	PVOID GdiDCAttributeList;
	PVOID LoaderLock;
	ULONG OSMajorVersion;
	ULONG OSMinorVersion;
	ULONG OSBuildNumber;
	ULONG OSPlatformId;
	ULONG ImageSubSystem;
	ULONG ImageSubSystemMajorVersion;
	ULONG ImageSubSystemMinorVersion;
	ULONG GdiHandleBuffer[0x22];
	ULONG PostProcessInitRoutine;
	ULONG TlsExpansionBitmap;
	BYTE TlsExpansionBitmapBits[0x80];
	ULONG SessionId;
}
typedef PEB, *PPEB;

extern ULONG(NTAPI* RtlNtStatusToDosError)(
	_In_ NTSTATUS Status);

extern NTSTATUS(NTAPI* RtlSetCurrentDirectory_U)(
	_In_ PUNICODE_STRING PathName);

extern NTSTATUS(NTAPI* NtWaitForSingleObject)(
	_In_ HANDLE Handle,
	_In_ BOOLEAN Alertable,
	_In_opt_ PLARGE_INTEGER Timeout);

extern NTSTATUS(NTAPI* NtQueryInformationFile)(
	_In_ HANDLE FileHandle,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_Out_ PVOID FileInformation,
	_In_ ULONG Length,
	_In_ FILE_INFORMATION_CLASS FileInformationClass);

extern NTSTATUS(NTAPI* NtSetInformationFile)(
	_In_ HANDLE FileHandle,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_ PVOID FileInformation,
	_In_ ULONG Length,
	_In_ FILE_INFORMATION_CLASS FileInformationClass);

extern NTSTATUS(NTAPI* NtCreateFile)(
	_Out_ PHANDLE FileHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_opt_ PLARGE_INTEGER AllocationSize,
	_In_ ULONG FileAttributes,
	_In_ ULONG ShareAccess,
	_In_ ULONG CreateDisposition,
	_In_ ULONG CreateOptions,
	_In_opt_ PVOID EaBuffer,
	_In_ ULONG EaLength);

extern NTSTATUS(NTAPI* NtReadFile)(
	_In_ HANDLE FileHandle,
	_In_opt_ HANDLE Event,
	_In_opt_ PIO_APC_ROUTINE ApcRoutine,
	_In_opt_ PVOID ApcContext,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_Out_ PVOID Buffer,
	_In_ ULONG Length,
	_In_opt_ PLARGE_INTEGER ByteOffset,
	_In_opt_ PULONG Key);

extern NTSTATUS(NTAPI* NtWriteFile)(
	_In_ HANDLE FileHandle,
	_In_opt_ HANDLE Event,
	_In_opt_ PIO_APC_ROUTINE ApcRoutine,
	_In_opt_ PVOID ApcContext,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_ PVOID Buffer,
	_In_ ULONG Length,
	_In_opt_ PLARGE_INTEGER ByteOffset,
	_In_opt_ PULONG Key);

extern NTSTATUS(NTAPI* NtCancelIoFileEx)(
	_In_ HANDLE FileHandle,
	_Out_ PIO_STATUS_BLOCK IoRequestToCancel,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock);

extern NTSTATUS(NTAPI* NtRemoveIoCompletion)(
	_In_ HANDLE IoCompletionHandle,
	_Out_ PVOID *CompletionKey,
	_Out_ PVOID *ApcContext,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_opt_ PLARGE_INTEGER Timeout);

extern NTSTATUS(NTAPI* NtRemoveIoCompletionEx)(
	_In_ HANDLE IoCompletionHandle,
	_Out_writes_to_(Count, *NumEntriesRemoved) PFILE_IO_COMPLETION_INFORMATION IoCompletionInformation,
	_In_ ULONG Count,
	_Out_ PULONG NumEntriesRemoved,
	_In_opt_ PLARGE_INTEGER Timeout,
	_In_ BOOLEAN Alertable);

extern NTSTATUS(NTAPI* NtCreateWaitCompletionPacket)(
	_Out_ PHANDLE WaitCompletionPacketHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_opt_ POBJECT_ATTRIBUTES ObjectAttributes);

extern NTSTATUS(NTAPI* NtAssociateWaitCompletionPacket)(
	_In_ HANDLE WaitCompletionPacketHandle,
	_In_ HANDLE IoCompletionHandle,
	_In_ HANDLE TargetObjectHandle,
	_In_opt_ PVOID KeyContext,
	_In_opt_ PVOID ApcContext,
	_In_ NTSTATUS IoStatus,
	_In_ ULONG_PTR IoStatusInformation,
	_Out_opt_ PBOOLEAN AlreadySignaled);

extern NTSTATUS(NTAPI* NtCancelWaitCompletionPacket)(
	_In_ HANDLE WaitCompletionPacketHandle,
	_In_ BOOLEAN RemoveSignaledPacket);

extern NTSTATUS(NTAPI* NtQueryDirectoryFileEx)(
	_In_ HANDLE FileHandle,
	_In_opt_ HANDLE Event,
	_In_opt_ PIO_APC_ROUTINE ApcRoutine,
	_In_opt_ PVOID ApcContext,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_Out_writes_bytes_(Length) PVOID FileInformation,
	_In_ ULONG Length,
	FILE_INFORMATION_CLASS FileInformationClass,
	_In_ ULONG QueryFlags,
	_In_opt_ PUNICODE_STRING FileName);

extern NTSTATUS(NTAPI* NtQueryFullAttributesFile)(
	_In_ POBJECT_ATTRIBUTES ObjectAttributes,
	_Out_ PFILE_NETWORK_OPEN_INFORMATION FileInformation);

extern NTSTATUS(NTAPI* NtAllocateVirtualMemory)(
	_In_ HANDLE ProcessHandle,
	_Inout_ PVOID* BaseAddress,
	_In_ ULONG_PTR ZeroBits,
	_Inout_ PSIZE_T RegionSize,
	_In_ ULONG AllocationType,
	_In_ ULONG Protect);

extern NTSTATUS(NTAPI* NtFreeVirtualMemory)(
	_In_ HANDLE ProcessHandle,
	_Inout_ PVOID* BaseAddress,
	_Inout_ PSIZE_T RegionSize,
	_In_ ULONG FreeType);

extern NTSTATUS(NTAPI* NtProtectVirtualMemory)(
	_In_ HANDLE ProcessHandle,
	_Inout_ PVOID* BaseAddress,
	_Inout_ PSIZE_T RegionSize,
	_In_ ULONG NewProtect,
	_Out_ PULONG OldProtect);

extern NTSTATUS(NTAPI* NtOpenSection)(
	_Out_ PHANDLE SectionHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes);

extern NTSTATUS(NTAPI* NtCreateSection)(
	_Out_ PHANDLE SectionHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_opt_ PLARGE_INTEGER MaximumSize,
	_In_ ULONG SectionPageProtection,
	_In_ ULONG AllocationAttributes,
	_In_opt_ HANDLE FileHandle);

extern NTSTATUS(NTAPI* NtMapViewOfSection)(
	_In_ HANDLE SectionHandle
	_In_ HANDLE ProcessHandle,
	_Inout_ PVOID* BaseAddress,
	_In_ ULONG_PTR ZeroBits,
	_In_ SIZE_T CommitSize,
	_Inout_opt_ PLARGE_INTEGER SectionOffset,
	_In_opt_ PSIZE_T RegionSize,
	_In_ SECTION_INHERIT InheritDisposition,
	_In_ ULONG AllocationType,
	_In_ ULONG Protect);

extern NTSTATUS(NTAPI* NtUnmapViewOfSection)(
	_In_ HANDLE ProcessHandle,
	_In_opt_ PVOID BaseAddress);


inline TEB* NtCurrentTeb()
{
#ifdef _WIN64
	return reinterpret_cast<TEB*>(__readgsqword(0x30));
#else
	return reinterpret_Cast<TEB*>(__readfsdword(0x18));
#endif
}

inline PEB* NtCurrentPeb()
{
#ifdef _WIN64
	return reinterpret_cast<PEB*>(__readgsqword(0x60));
#else
	return reinterpret_cast<PEB*>(__readfsdword(0x30));
#endif
}


vsm::result<void> kernel_init();

inline IO_STATUS_BLOCK make_io_status_block()
{
	return
	{
		.Status = -1,
		.Information = 0,
	};
}

NTSTATUS io_wait(HANDLE handle, IO_STATUS_BLOCK* isb, deadline deadline);
NTSTATUS io_cancel(HANDLE handle, IO_STATUS_BLOCK* isb);


class kernel_timeout
{
	using duration = std::chrono::duration<uint64_t, std::ratio<1, 10000000>>;

	LARGE_INTEGER m_timeout;
	LARGE_INTEGER* m_p_timeout;

public:
	kernel_timeout()
		: m_p_timeout(nullptr)
	{
	}

	kernel_timeout(deadline const deadline)
	{
		set(deadline);
	}

	kernel_timeout(kernel_timeout const&) = delete;
	kernel_timeout& operator=(kernel_timeout const&) = delete;

	LARGE_INTEGER* get()
	{
		return m_p_timeout;
	}

	LARGE_INTEGER const* get() const
	{
		return m_p_timeout;
	}

	void set(deadline const deadline)
	{
		vsm_assert(deadline.is_trivial() || deadline.is_relative());
		if (deadline == allio::deadline::never())
		{
			m_p_timeout = nullptr;
		}
		else
		{
			m_timeout.QuadPart =
				deadline == allio::deadline::instant()
					? 0
					: static_cast<uint64_t>(-static_cast<int64_t>(
						std::chrono::duration_cast<duration>(deadline.relative()).count()));
			m_p_timeout = &m_timeout;
		}
	}

	void set_instant()
	{
		m_timeout.QuadPart = 0;
		m_p_timeout = &m_timeout;
	}

	void set_never()
	{
		m_p_timeout = nullptr;
	}

	operator LARGE_INTEGER*()
	{
		return m_p_timeout;
	}

	operator LARGE_INTEGER const*() const
	{
		return m_p_timeout;
	}
};

} // namespace allio::win32
