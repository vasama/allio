#include <allio/detail/handles/pipe.hpp>

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/win32/byte_io.hpp>
#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/handles/platform_object.hpp>
#include <allio/impl/win32/kernel.hpp>

#include <vsm/lazy.hpp>
#include <vsm/numeric.hpp>
#include <vsm/out_resource.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

static vsm::result<HANDLE> get_named_pipe_directory()
{
	static constexpr auto open = []() -> vsm::result<unique_handle>
	{
		UNICODE_STRING path = win32::make_unicode_string(L"\\Device\\NamedPipe\\");

		OBJECT_ATTRIBUTES object_attributes = {};
		object_attributes.Length = sizeof(object_attributes);
		object_attributes.ObjectName = &path;

		IO_STATUS_BLOCK io_status_block;

		unique_handle handle;
		NTSTATUS const status = win32::NtCreateFile(
			vsm::out_resource(handle),
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

		return handle;
	};

	static auto const handle = open();
	vsm_try((auto const&, h), handle);
	return h.get();
}

static vsm::result<handle_with_flags> create_named_pipe_file(io_flags const flags)
{
	vsm_try(directory, get_named_pipe_directory());

	UNICODE_STRING name = {};

	OBJECT_ATTRIBUTES object_attributes = {};
	object_attributes.Length = sizeof(object_attributes);
	object_attributes.RootDirectory = directory;
	object_attributes.ObjectName = &name;
	object_attributes.Attributes = OBJ_CASE_INSENSITIVE;

	if (vsm::any_flags(flags, io_flags::create_inheritable))
	{
		object_attributes.Attributes |= OBJ_INHERIT;
	}

	ULONG create_options = 0;
	handle_flags h_flags = handle_flags::none;

	if (vsm::no_flags(flags, io_flags::create_non_blocking))
	{
		create_options |= FILE_SYNCHRONOUS_IO_NONALERT;
		h_flags |= platform_object_t::impl_type::flags::synchronous;
	}

	IO_STATUS_BLOCK io_status_block;

	unique_handle handle;
	NTSTATUS const status = NtCreateNamedPipeFile(
		vsm::out_resource(handle),
		GENERIC_READ | SYNCHRONIZE | FILE_WRITE_ATTRIBUTES,
		&object_attributes,
		&io_status_block,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_CREATE,
		create_options,
		/* NamedPipeType: */ 0,
		/* ReadMode: */ 0,
		/* CompletionMode: */ 0,
		/* MaximumInstances: */ 1,
		/* InboundQuota: */ 4096,
		/* OutboundQuota: */ 4096,
		kernel_timeout(std::chrono::milliseconds(INFINITE)));
	vsm_assert(status != STATUS_PENDING);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	if ((create_options & FILE_SYNCHRONOUS_IO_NONALERT) == 0)
	{
		h_flags |= set_file_completion_notification_modes(handle.get());
	}

	return vsm_lazy(handle_with_flags{ vsm_move(handle), h_flags });
}

static vsm::result<handle_with_flags> create_pipe(
	HANDLE const server_handle,
	io_flags const flags)
{
	UNICODE_STRING name = {};

	OBJECT_ATTRIBUTES object_attributes = {};
	object_attributes.Length = sizeof(object_attributes);
	object_attributes.RootDirectory = server_handle;
	object_attributes.ObjectName = &name;
	object_attributes.Attributes = OBJ_CASE_INSENSITIVE;

	if (vsm::any_flags(flags, io_flags::create_inheritable))
	{
		object_attributes.Attributes |= OBJ_INHERIT;
	}

	ULONG create_options = FILE_NON_DIRECTORY_FILE;
	handle_flags h_flags = handle_flags::none;

	if (vsm::no_flags(flags, io_flags::create_non_blocking))
	{
		create_options |= FILE_SYNCHRONOUS_IO_NONALERT;
		h_flags |= platform_object_t::impl_type::flags::synchronous;
	}

	IO_STATUS_BLOCK io_status_block;

	unique_handle handle;
	NTSTATUS const status = win32::NtCreateFile(
		vsm::out_resource(handle),
		GENERIC_WRITE | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
		&object_attributes,
		&io_status_block,
		/* AllocationSize: */ nullptr,
		/* FileAttributes: */ 0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_OPEN,
		create_options,
		/* EaBuffer: */ nullptr,
		/* EaLength: */ 0);
	vsm_assert(status != STATUS_PENDING);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	if ((create_options & FILE_SYNCHRONOUS_IO_NONALERT) == 0)
	{
		h_flags |= set_file_completion_notification_modes(handle.get());
	}

	return vsm_lazy(handle_with_flags{ vsm_move(handle), h_flags });
}

vsm::result<basic_detached_handle<pipe_t>> pipe_t::create_pair(
	native_handle<pipe_t>& h,
	io_parameters_t<pipe_t, create_pair_t> const& a)
{
#if 0
	SECURITY_ATTRIBUTES security_attributes = {};
	security_attributes.nLength = sizeof(security_attributes);

	if (vsm::any_flags(a.flags, io_flags::create_inheritable))
	{
		security_attributes.bInheritHandle = true;
	}

	unique_handle read_handle;
	unique_handle write_handle;
	if (!CreatePipe(
		vsm::out_resource(read_handle),
		vsm::out_resource(write_handle),
		&security_attributes,
		/* nSize (buffer size): */ 0))
	{
		return vsm::unexpected(get_last_error());
	}
#endif

	vsm_try_bind((server_handle, server_flags), ::create_named_pipe_file(a.read_pipe.flags));
	vsm_try_bind((client_handle, client_flags), ::create_pipe(server_handle.get(), a.write_pipe.flags));

	h.flags = flags::not_null;
	h.platform_handle = wrap_handle(server_handle.release());

	native_handle<pipe_t> w_h = {};
	w_h.flags = flags::not_null;
	w_h.platform_handle = wrap_handle(client_handle.release());

	return vsm::result<basic_detached_handle<pipe_t>>(
		vsm::result_value,
		adopt_handle,
		w_h);
}

vsm::result<size_t> pipe_t::stream_read(
	native_handle<pipe_t> const& h,
	io_parameters_t<pipe_t, stream_read_t> const& a)
{
	return win32::stream_read(h, a);
}

vsm::result<size_t> pipe_t::stream_write(
	native_handle<pipe_t> const& h,
	io_parameters_t<pipe_t, stream_write_t> const& a)
{
	return win32::stream_write(h, a);
}
