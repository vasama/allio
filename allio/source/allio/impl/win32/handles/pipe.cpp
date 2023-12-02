#include <allio/detail/handles/pipe.hpp>

#include <allio/impl/win32/byte_io.hpp>
#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/detail/unique_handle.hpp>
#include <allio/win32/platform.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

static UNICODE_STRING make_unicode_string(std::wstring_view const string)
{
	vsm_assert(string.size() <= 0x7fff); //PRECONDITION

	UNICODE_STRING unicode_string;
	unicode_string.Buffer = const_cast<wchar_t*>(string.data());
	unicode_string.Length = string.size() * sizeof(wchar_t);
	unicode_string.MaximumLength = unicode_string.Length;

	return unicode_string;
}

static vsm::result<HANDLE> get_named_pipe_directory()
{
	static constexpr auto open = []() -> vsm::result<unique_handle>
	{
		UNICODE_STRING path = make_unicode_string(L"\\Device\\NamedPipe\\");

		OBJECT_ATTRIBUTES object_attributes = {};
		object_attributes.Length = sizeof(object_attributes);
		object_attributes.ObjectName = &path;

		IO_STATUS_BLOCK io_status_block;

		HANDLE handle;
		NTSTATUS const status = win32::NtCreateFile(
			&handle,
			GENERIC_READ | SYNCHRONIZE,
			&object_attributes,
			&io_status_block,
			/* AllocationSize: */ nullptr,
			/* FileAttributes: */ 0,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			FILE_OPEN,
			FILE_SYNCHRONOUS_IO_NONALERT,
			/* EaBuffer: */ nullptr,
			/* EaLength: */ 0);
		vsm_assert(status != STATUS_PENDING);

		if (!NT_SUCCESS(status))
		{
			return vsm::unexpected(static_cast<kernel_error>(status));
		}

		return vsm_lazy(unique_handle(handle));
	};

	static auto const handle = open();
	vsm_try((auto const&, h), handle);
	return h.get();
}

static vsm::result<unique_handle> create_named_pipe_file(
	bool const inheritable)
{
	vsm_try(directory, get_named_pipe_directory());

	UNICODE_STRING name = {};

	OBJECT_ATTRIBUTES object_attributes = {};
	object_attributes.Length = sizeof(object_attributes);
	object_attributes.RootDirectory = directory;
	object_attributes.ObjectName = &name;
	object_attributes.Attributes = OBJ_CASE_INSENSITIVE;

	if (inheritable)
	{
		object_attributes.Attributes |= OBJ_INHERIT;
	}

	IO_STATUS_BLOCK io_status_block;

	HANDLE handle;
	NTSTATUS const status = NtCreateNamedPipeFile(
		&handle,
		GENERIC_READ | SYNCHRONIZE | FILE_WRITE_ATTRIBUTES,
		&object_attributes,
		&io_status_block,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_CREATE,
		FILE_SYNCHRONOUS_IO_NONALERT,
		/* NamedPipeType: */ 0,
		/* ReadMode: */ 0,
		/* CompletionMode: */ 0,
		/* MaximumInstances: */ 1,
		/* InboundQuota: */ 4096,
		/* OutboundQuota: */ 4096,
		/* DefaultTimeout: */ nullptr);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return vsm_lazy(unique_handle(handle));
}

static vsm::result<unique_handle> create_pipe(
	HANDLE const server_handle,
	bool const inheritable)
{
	UNICODE_STRING name = {};

	OBJECT_ATTRIBUTES object_attributes = {};
	object_attributes.Length = sizeof(object_attributes);
	object_attributes.RootDirectory = server_handle;
	object_attributes.ObjectName = &name;
	object_attributes.Attributes = OBJ_CASE_INSENSITIVE;

	if (inheritable)
	{
		object_attributes.Attributes |= OBJ_INHERIT;
	}

	IO_STATUS_BLOCK io_status_block;

	HANDLE handle;
	NTSTATUS const status = win32::NtCreateFile(
		&handle,
		GENERIC_WRITE | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
		&object_attributes,
		&io_status_block,
		/* AllocationSize: */ nullptr,
		/* FileAttributes: */ 0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_OPEN,
		FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
		/* EaBuffer: */ nullptr,
		/* EaLength: */ 0);
	vsm_assert(status != STATUS_PENDING);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return vsm_lazy(unique_handle(handle));
}

vsm::result<void> pipe_t::create(native_type& r_h, native_type& w_h)
{
	HANDLE read_handle;
	HANDLE write_handle;

	SECURITY_ATTRIBUTES security_attributes = {};
	security_attributes.nLength = sizeof(security_attributes);
	security_attributes.bInheritHandle = true;

	if (!CreatePipe(
		&read_handle,
		&write_handle,
		&security_attributes,
		/* nSize (buffer size): */ 0))
	{
		return vsm::unexpected(get_last_error());
	}

	r_h = platform_object_t::native_type
	{
		object_t::native_type
		{
			flags::not_null,
		},
		wrap_handle(read_handle),
	};

	w_h = platform_object_t::native_type
	{
		object_t::native_type
		{
			flags::not_null,
		},
		wrap_handle(write_handle),
	};
	
	return {};
}

vsm::result<size_t> pipe_t::stream_read(
	native_type const& h,
	io_parameters_t<pipe_t, read_some_t> const& a)
{
	return win32::stream_read(h, a);
}

vsm::result<size_t> pipe_t::stream_write(
	native_type const& h,
	io_parameters_t<pipe_t, write_some_t> const& a)
{
	return win32::stream_write(h, a);
}
