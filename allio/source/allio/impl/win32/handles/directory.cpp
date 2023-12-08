#include <allio/handles/directory.hpp>
#include <allio/impl/win32/handles/directory.hpp>

#include <allio/impl/transcode.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/peb.hpp>
#include <allio/win32/kernel_error.hpp>

#include <vsm/numeric.hpp>

#include <bit>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using directory_stream_entry = FILE_ID_FULL_DIR_INFORMATION;
static constexpr auto directory_stream_information = FileIdFullDirectoryInformation;

static fs_entry_type get_entry_type(directory_stream_entry const& entry)
{
	if (entry.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
	{
		if (entry.ReparsePointTag == IO_REPARSE_TAG_SYMLINK)
		{
			return fs_entry_type::symbolic_link;
		}

		if (entry.ReparsePointTag == IO_REPARSE_TAG_MOUNT_POINT)
		{
			return fs_entry_type::ntfs_junction;
		}
	}

	if (entry.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		return fs_entry_type::directory;
	}
	else
	{
		return fs_entry_type::regular;
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

	if (size_t const next_offset = entry->NextEntryOffset)
	{
		return reinterpret_cast<directory_stream_entry const*>(
			reinterpret_cast<std::byte const*>(entry) + next_offset);
	}

	return nullptr;
}


vsm::result<size_t> directory_entry::get_name(any_string_buffer const buffer) const
{
	return transcode_string(name.view<wchar_t>(), buffer);
}

directory_entry directory_entry_view::get_entry(directory_stream_native_handle const handle)
{
	vsm_assert(handle != directory_stream_native_handle::end);
	directory_stream_entry const& entry = *unwrap_stream(handle);

	return
	{
		.type = get_entry_type(entry),
		.node_id = std::bit_cast<fs_node_id>(entry.FileId),
		.name = any_string_view(get_entry_name(entry)),
	};
}

directory_stream_native_handle directory_stream_iterator::advance(directory_stream_native_handle const handle)
{
	return wrap_stream(next_entry(unwrap_stream(handle)));
}


static bool filter_entry(directory_stream_entry const& entry)
{
	std::wstring_view const name = get_entry_name(entry);

	if (name[0] == L'.')
	{
		size_t const size = name.size();

		if (size == 1)
		{
			return false;
		}

		if (size == 2 && name[1] == L'.')
		{
			return false;
		}
	}

	return true;
}

NTSTATUS win32::query_directory_file_start(
	HANDLE const handle,
	read_buffer const buffer,
	bool const restart,
	PIO_APC_ROUTINE const apc_routine,
	PVOID const apc_context,
	IO_STATUS_BLOCK& io_status_block)
{
	ULONG flags = 0;

	if (restart)
	{
		flags |= SL_RESTART_SCAN;
	}

	//TODO: Align args.buffer first.
	return NtQueryDirectoryFileEx(
		handle,
		/* Event: */ NULL,
		apc_routine,
		apc_context,
		&io_status_block,
		buffer.data(),
		vsm::saturating(buffer.size()),
		directory_stream_information,
		flags,
		/* FileName: */ nullptr);
}

NTSTATUS win32::query_directory_file_completed(
	read_buffer const buffer,
	IO_STATUS_BLOCK const& io_status_block,
	directory_stream_native_handle& out_stream)
{
	NTSTATUS const status = io_status_block.Status;

	if (!NT_SUCCESS(status))
	{
		if (status == STATUS_NO_MORE_FILES)
		{
			out_stream = directory_stream_native_handle::directory_end;
			return STATUS_SUCCESS;
		}

		return status;
	}

	if (io_status_block.Information == 0)
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

	auto entry = reinterpret_cast<directory_stream_entry const*>(buffer.data());

	while (entry != nullptr && !filter_entry(*entry))
	{
		entry = next_entry(entry);
	}

	out_stream = wrap_stream(entry);
	return STATUS_SUCCESS;
}

static vsm::result<directory_stream_native_handle> query_directory_file(
	HANDLE const handle,
	read_buffer const buffer,
	bool const restart)
{
	IO_STATUS_BLOCK io_status_block = make_io_status_block();

	NTSTATUS status = query_directory_file_start(
		handle,
		buffer,
		restart,
		/* apc_routine: */ nullptr,
		/* apc_context: */ nullptr,
		io_status_block);

	if (status == STATUS_PENDING)
	{
		status = io_wait(handle, &io_status_block, deadline::never());
	}
	vsm_assert(io_status_block.Status == status);

	directory_stream_native_handle stream;
	status = query_directory_file_completed(
		buffer,
		io_status_block,
		stream);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return stream;
}


vsm::result<void> directory_t::open(
	native_type& h,
	io_parameters_t<directory_t, open_t> const& a)
{
	vsm_try_bind((directory, flags), create_file(
		unwrap_handle(a.path.base),
		a.path.path,
		open_kind::directory,
		a));

	h = platform_object_t::native_type
	{
		object_t::native_type
		{
			flags::not_null | flags,
		},
		wrap_handle(directory.release()),
	};

	return {};
}

vsm::result<directory_stream_view> directory_t::read(
	native_type const& h,
	io_parameters_t<directory_t, read_t> const& a)
{
	vsm_try(stream, query_directory_file(
		unwrap_handle(h.platform_handle),
		a.buffer,
		a.restart));

	return directory_stream_view(stream);
}


static constexpr size_t max_unicode_string_size = 0x7FFE;
static vsm::result<UNICODE_STRING> make_unicode_path(platform_path_view const path)
{
	std::wstring_view const string = path.string();

	if (string.size() > max_unicode_string_size)
	{
		return vsm::unexpected(error::filename_too_long);
	}

	UNICODE_STRING unicode_string;
	unicode_string.Buffer = const_cast<wchar_t*>(string.data());
	unicode_string.Length = static_cast<USHORT>(string.size() * sizeof(wchar_t));
	unicode_string.MaximumLength = unicode_string.Length;
	return unicode_string;
}

static vsm::result<void> _set_current_directory(platform_path_view const path)
{
	vsm_try(unicode_string, make_unicode_path(path));

	NTSTATUS const status = RtlSetCurrentDirectory_U(&unicode_string);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}


vsm::result<size_t> this_process::get_current_directory(any_path_buffer const buffer)
{
	unique_peb_lock peb_lock;

	auto const& process_parameters = *NtCurrentPeb()->ProcessParameters;

	auto const wide_path = std::wstring_view(
		process_parameters.CurrentDirectoryPath.Buffer,
		process_parameters.CurrentDirectoryPath.Length / sizeof(wchar_t));

	return transcode_string(wide_path, buffer);
}

#if 0
vsm::result<void> this_process::set_current_directory(any_path_view const path)
{
	//TODO: Transcode into local buffer if necessary
	return _set_current_directory(wide_path);
}
#endif

#if 0
vsm::result<blocking::directory_handle> this_process::open_current_directory()
{
	unique_peb_lock lock;

	auto const& process_parameters = *NtCurrentPeb()->ProcessParameters;
	HANDLE const handle = process_parameters.CurrentDirectoryHandle;

	if (handle != NULL)
	{
		//TODO: reopen directory
	}
	else
	{
		//TODO: open directory
	}
}
#endif
