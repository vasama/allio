#include <allio/impl/win32/directory_handle.hpp>

#include <allio/impl/win32/encoding.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/peb.hpp>
#include <allio/impl/win32/sync_filesystem_handle.hpp>
#include <allio/win32/nt_error.hpp>

#include <vsm/utility.hpp>

#include <bit>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using directory_stream_entry = FILE_ID_FULL_DIR_INFORMATION;
static constexpr auto directory_stream_information = FileIdFullDirectoryInformation;

static file_kind get_entry_kind(directory_stream_entry const& entry)
{
	if (entry.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
	{
		if (entry.ReparsePointTag == IO_REPARSE_TAG_SYMLINK)
		{
			return file_kind::symbolic_link;
		}

		if (entry.ReparsePointTag == IO_REPARSE_TAG_MOUNT_POINT)
		{
			return file_kind::ntfs_junction;
		}
	}

	if (entry.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		return file_kind::directory;
	}
	else
	{
		return file_kind::regular;
	}
}

static std::wstring_view get_entry_name(directory_stream_entry const& entry)
{
	return std::wstring_view(entry.FileName, entry.FileNameLength / sizeof(wchar_t));
}


static directory_stream_native_handle wrap_stream(directory_stream_entry const* const stream)
{
	return static_cast<directory_stream_native_handle>(reinterpret_cast<uintptr_t>(stream));
}

static directory_stream_entry const* unwrap_stream(directory_stream_native_handle const handle)
{
	vsm_assert(handle != directory_stream_native_handle::directory_end);
	return reinterpret_cast<directory_stream_entry const*>(static_cast<uintptr_t>(handle));
}

static directory_stream_entry const* next_entry(directory_stream_entry const* const entry)
{
	vsm_assert(entry != nullptr);
	size_t const next_offset = entry->NextEntryOffset;
	return next_offset != 0
		? reinterpret_cast<directory_stream_entry const*>(
			reinterpret_cast<std::byte const*>(entry) + next_offset)
		: nullptr;
}


directory_entry directory_entry_view::get_entry(directory_stream_native_handle const handle)
{
	vsm_assert(handle != directory_stream_native_handle::end);
	directory_stream_entry const& entry = *unwrap_stream(handle);

	return
	{
		.kind = get_entry_kind(entry),
		.node_id = std::bit_cast<file_id_64>(entry.FileId),
		.name = input_string_view(get_entry_name(entry)),
	};
}

directory_stream_native_handle directory_stream_iterator::advance(directory_stream_native_handle const handle)
{
	return wrap_stream(next_entry(unwrap_stream(handle)));
}


static bool discard_entry(directory_stream_entry const& entry)
{
	std::wstring_view const name = get_entry_name(entry);

	if (name[0] == L'.')
	{
		size_t const size = name.size();

		if (size == 1)
		{
			return true;
		}

		if (size == 2 && name[1] == L'.')
		{
			return true;
		}
	}

	return false;
}

NTSTATUS win32::query_directory_file_start(
	io::parameters_with_result<io::directory_read> const& args,
	PIO_APC_ROUTINE const apc_routine, PVOID const apc_context,
	IO_STATUS_BLOCK& io_status_block)
{
	ULONG flags = 0;

	if (args.restart)
	{
		flags |= SL_RESTART_SCAN;
	}

	//TODO: Align args.buffer first.
	return NtQueryDirectoryFileEx(
		unwrap_handle(args.handle->get_platform_handle()),
		NULL,
		apc_routine,
		apc_context,
		&io_status_block,
		args.buffer.data(),
		args.buffer.size(),
		FileIdExtdDirectoryInformation,
		flags,
		nullptr);
}

NTSTATUS win32::query_directory_file_completed(
	io::parameters_with_result<io::directory_read> const& args,
	IO_STATUS_BLOCK const& io_status_block)
{
	NTSTATUS const status = io_status_block.Status;

	if (!NT_SUCCESS(status))
	{
		if (status == STATUS_NO_MORE_FILES)
		{
			*args.result = directory_stream_view(directory_stream_native_handle::directory_end);
			return STATUS_SUCCESS;
		}

		return status;
	}

	if (io_status_block.Information == 0)
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

	auto entry = reinterpret_cast<directory_stream_entry const*>(args.buffer.data());

	while (entry != nullptr && discard_entry(*entry))
	{
		entry = next_entry(entry);
	}

	*args.result = directory_stream_view(wrap_stream(entry));
	return status;
}

static vsm::result<void> query_directory_file(
	io::parameters_with_result<io::directory_read> const& args)
{
	IO_STATUS_BLOCK io_status_block = make_io_status_block();

	NTSTATUS status = query_directory_file_start(args, nullptr, nullptr, io_status_block);

	if (status == STATUS_PENDING)
	{
		HANDLE const handle = unwrap_handle(args.handle->get_platform_handle());
		status = io_wait(handle, &io_status_block, deadline::never());
	}

	status = query_directory_file_completed(args, io_status_block);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<nt_error>(status));
	}

	return {};
}


vsm::result<void> directory_handle_base::sync_impl(io::parameters_with_result<io::filesystem_open> const& args)
{
	return sync_open<directory_handle>(args, open_kind::directory);
}

vsm::result<void> directory_handle_base::sync_impl(io::parameters_with_result<io::directory_read> const& args)
{
	directory_handle const& h = *args.handle;

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	IO_STATUS_BLOCK io_status_block = make_io_status_block();

	NTSTATUS status = query_directory_file_start(args, nullptr, nullptr, io_status_block);

	if (status == STATUS_PENDING)
	{
		HANDLE const handle = unwrap_handle(args.handle->get_platform_handle());
		status = io_wait(handle, &io_status_block, deadline::never());
	}

	status = query_directory_file_completed(args, io_status_block);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<nt_error>(status));
	}

	return {};
}

allio_handle_implementation(directory_handle);


vsm::result<size_t> this_process::get_current_directory(output_path_ref const output)
{
	unique_peb_lock peb_lock;

	auto const& process_parameters = *NtCurrentPeb()->ProcessParameters;

	auto const wide_path = std::wstring_view(
		process_parameters.CurrentDirectoryPath.Buffer,
		process_parameters.CurrentDirectoryPath.Length / sizeof(wchar_t));

	return copy_string(wide_path, output);
}

static vsm::result<void> set_current_directory_impl(std::wstring_view const path)
{
	UNICODE_STRING unicode_string;
	unicode_string.Buffer = const_cast<wchar_t*>(path.data());
	unicode_string.Length = path.size() * sizeof(wchar_t);
	unicode_string.MaximumLength = unicode_string.Length;

	NTSTATUS const status = RtlSetCurrentDirectory_U(&unicode_string);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<nt_error>(status));
	}

	return {};
}

#if 0
vsm::result<void> this_process::set_current_directory(input_path_view const path)
{
	if (path.is_wide())
	{
		return set_current_directory_impl(path.wide_view());
	}
}

vsm::result<directory_handle> this_process::open_current_directory()
{
	unique_peb_lock lock;

	auto const& process_parameters = *NtCurrentPeb()->ProcessParameters;

	//TODO: reopen process_parameters.CurrentDirectoryHandle
}
#endif
