#include <allio/detail/handles/directory.hpp>
#include <allio/impl/win32/handles/directory.hpp>

#include <allio/impl/transcode.hpp>
#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/peb.hpp>
#include <allio/impl/win32/thread_event.hpp>
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

		//TODO: Check if this is correct.
		return fs_entry_type::unknown;
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


static directory_stream_pointer wrap_stream(directory_stream_entry const* const stream)
{
	return stream == nullptr
		? directory_stream_pointer::end_of_stream
		: static_cast<directory_stream_pointer>(reinterpret_cast<uintptr_t>(stream));
}

static directory_stream_entry const* unwrap_stream(directory_stream_pointer const pointer)
{
	vsm_assert(pointer != directory_stream_pointer::end_of_directory);
	return pointer == directory_stream_pointer::end_of_stream
		? nullptr
		: reinterpret_cast<directory_stream_entry const*>(static_cast<uintptr_t>(pointer));
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


directory_stream_pointer detail::next_directory_entry(directory_stream_pointer const pointer)
{
	return wrap_stream(next_entry(unwrap_stream(pointer)));
}

fs_entry_type detail::get_directory_entry_type(directory_stream_pointer const pointer)
{
	vsm_assert(pointer != directory_stream_pointer::end_of_stream);
	return get_entry_type(*unwrap_stream(pointer));
}

std::wstring_view detail::get_directory_entry_name(directory_stream_pointer const pointer)
{
	vsm_assert(pointer != directory_stream_pointer::end_of_stream);
	return get_entry_name(*unwrap_stream(pointer));
}

vsm::result<size_t> detail::copy_directory_entry_name(
	directory_stream_pointer const pointer,
	any_string_buffer const buffer)
{
	return transcode_string(get_directory_entry_name(pointer), buffer);
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
	HANDLE const event,
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
		event,
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
	NTSTATUS const status,
	ULONG_PTR const information,
	directory_stream_pointer& out_pointer)
{
	vsm_assert(status != STATUS_PENDING);

	if (!NT_SUCCESS(status))
	{
		if (status == STATUS_NO_MORE_FILES)
		{
			out_pointer = directory_stream_pointer::end_of_directory;
			return STATUS_SUCCESS;
		}

		return status;
	}

	if (information == 0)
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

	auto entry = reinterpret_cast<directory_stream_entry const*>(buffer.data());

	while (entry != nullptr && !filter_entry(*entry))
	{
		entry = next_entry(entry);
	}

	out_pointer = wrap_stream(entry);
	return STATUS_SUCCESS;
}

static vsm::result<directory_stream_pointer> query_directory_file(
	HANDLE const handle,
	thread_event& event,
	read_buffer const buffer,
	bool const restart)
{
	if (buffer.size() <= sizeof(FILE_ID_FULL_DIR_INFORMATION))
	{
		return vsm::unexpected(allio_error(error::no_buffer_space));
	}

	thread_event::io_status_block_t io_status_block;
	NTSTATUS status = query_directory_file_start(
		handle,
		event,
		buffer,
		restart,
		/* apc_routine: */ nullptr,
		/* apc_context: */ nullptr,
		io_status_block);

	if (status == STATUS_PENDING)
	{
		//TODO: Deadline
		status = event.wait_for_io(handle, io_status_block, deadline::never());
		vsm_assert(io_status_block.Status == status);
	}

	directory_stream_pointer stream_pointer;
	status = query_directory_file_completed(
		buffer,
		status,
		io_status_block.Information,
		stream_pointer);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

	return stream_pointer;
}


vsm::result<basic_directory_stream_view<void>> directory_t::read(
	native_handle<directory_t> const& h,
	io_parameters_t<directory_t, read_t> const& a)
{
	vsm_try(event, thread_event::get_for(h));

	vsm_try(stream, query_directory_file(
		unwrap_handle(h.platform_handle),
		event,
		a.buffer,
		/* restart: */ false));

	return basic_directory_stream_view<void>(stream);
}


#if 0
static constexpr size_t max_unicode_string_size = 0x7FFE;
static vsm::result<UNICODE_STRING> make_unicode_path(platform_path_view const path)
{
	std::wstring_view const string = path.string();

	if (string.size() > max_unicode_string_size)
	{
		return vsm::unexpected(allio_error(error::filename_too_long));
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
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
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
#endif

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


vsm::result<size_t> detail::_get_current_directory(any_path_buffer const buffer)
{
	unique_peb_lock peb_lock;

	auto const& process_parameters = *NtCurrentPeb()->ProcessParameters;

	auto const wide_string = std::wstring_view(
		process_parameters.CurrentDirectoryPath.Buffer,
		process_parameters.CurrentDirectoryPath.Length / sizeof(wchar_t));

	auto const wide_path = wpath_view(wide_string).without_trailing_separators();

	return transcode_string(wide_path.string(), buffer);
}


vsm::result<void> detail::_set_current_directory(fs_path const& path)
{
	static constexpr size_t path_storage_size =
		MAX_PATH // SetCurrentDirectoryW only accepts paths of at most MAX_PATH characters.
		+ 4 // UNC prefix ( "\\?\" ) written by get_current_path.
		+ 1 // Null terminator required by SetCurrentDirectoryW.
		;

	wchar_t path_storage[path_storage_size];

	wchar_t* path_beg = path_storage;
	wchar_t* base_path_end = path_storage;

	if (path.base != nullptr)
	{
		vsm_try(base_path_size, fs_object_t::get_current_path(
			*path.base,
			fs_io::get_current_path_t::params_type
			{
				.buffer = path_storage,
				.kind = path_kind::windows_dos,
			}));

		base_path_end = path_storage + base_path_size;
	}

	wchar_t* path_end = base_path_end;

	if (!path.path.empty())
	{
		vsm_try(relative_path_size, transcode_string(
			path.path.string(),
			string_buffer<wchar_t>(base_path_end, std::end(path_storage))));

		path_end = base_path_end + relative_path_size;
	}

	vsm_assert(path_end < std::end(path_storage));
	*path_end++ = L'\0';

	if (std::wstring_view(path_beg, path_end).starts_with(L"\\\\?\\"))
	{
		path_beg += 4;
	}

	if (!SetCurrentDirectoryW(path_beg))
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	return {};
}

static vsm::result<handle_with_flags> open_current_directory_from_peb()
{
	unique_peb_lock peb_lock;

	auto const& process_parameters = *NtCurrentPeb()->ProcessParameters;

	open_info const info =
	{
		//TODO: Is SYNCHRONIZE needed?
		.desired_access = SYNCHRONIZE,
		.create_disposition = FILE_OPEN,
		.create_options = FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
	};

	if (process_parameters.CurrentDirectoryHandle != NULL)
	{
		return reopen_file(process_parameters.CurrentDirectoryHandle, info);
	}
	else
	{
		return create_file(NULL, process_parameters.CurrentDirectoryPath, info);;
	}
}

vsm::result<basic_detached_handle<directory_t>> detail::_open_current_directory()
{
	vsm_try_bind((handle, flags), open_current_directory_from_peb());

	native_handle<directory_t> h = {};

	h.flags = object_t::flags::not_null | flags;
	h.platform_handle = wrap_handle(handle.release());

	return vsm::result<basic_detached_handle<directory_t>>(
		vsm::result_value,
		adopt_handle,
		h);
}
