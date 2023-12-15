#include <allio/impl/win32/handles/fs_object.hpp>

#include <allio/impl/new.hpp>
#include <allio/impl/transcode.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/kernel_path.hpp>
#include <allio/win32/kernel_error.hpp>

#include <vsm/lazy.hpp>
#include <vsm/out_resource.hpp>
#include <vsm/standard.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<open_info> open_info::make(open_parameters const& args)
{
	open_info info =
	{
		//TODO: Is synchronize needed for path_handle?
		.desired_access = SYNCHRONIZE,
	};

	if (vsm::no_flags(args.flags, io_flags::create_multiplexable))
	{
		info.create_options |= FILE_SYNCHRONOUS_IO_NONALERT;
	}

	switch (args.special & open_options::kind_mask)
	{
	case open_options::path:
		if (args.mode != file_mode::none)
		{
			return vsm::unexpected(error::invalid_argument);
		}
		break;

	case open_options::file:
		info.create_options |= FILE_NON_DIRECTORY_FILE;
		break;

	case open_options::directory:
		info.create_options |= FILE_DIRECTORY_FILE;
		break;

	default:
		return vsm::unexpected(error::invalid_argument);
	}

	if (args.mode != file_mode::none)
	{
		info.desired_access |= READ_CONTROL;
	}
	if (vsm::any_flags(args.mode, file_mode::read_data))
	{
		info.desired_access |= FILE_GENERIC_READ;
	}
	if (vsm::any_flags(args.mode, file_mode::write_data))
	{
		info.desired_access |= FILE_GENERIC_WRITE | DELETE;
	}
	if (vsm::any_flags(args.mode, file_mode::read_attributes))
	{
		info.desired_access |= FILE_READ_ATTRIBUTES | FILE_READ_EA;
	}
	if (vsm::any_flags(args.mode, file_mode::write_attributes))
	{
		info.desired_access |= FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA;
	}

	switch (args.opening)
	{
	case file_opening::open_existing:
		info.create_disposition = FILE_OPEN;
		break;

	case file_opening::create_only:
		info.create_disposition = FILE_CREATE;
		break;

	case file_opening::open_or_create:
		info.create_disposition = FILE_OPEN_IF;
		break;

	case file_opening::truncate_existing:
		info.create_disposition = FILE_OVERWRITE;
		break;

	case file_opening::replace_existing:
		info.create_disposition = FILE_SUPERSEDE;
		break;

	default:
		return vsm::unexpected(error::invalid_argument);
	}

	// Sharing
	if (vsm::any_flags(args.sharing, file_sharing::unlink))
	{
		info.share_access |= FILE_SHARE_DELETE;
	}
	if (vsm::any_flags(args.sharing, file_sharing::read))
	{
		info.share_access |= FILE_SHARE_READ;
	}
	if (vsm::any_flags(args.sharing, file_sharing::write))
	{
		info.share_access |= FILE_SHARE_WRITE;
	}

	return info;
}

static vsm::result<handle_with_flags> _create_file(
	HANDLE const root_handle,
	UNICODE_STRING path,
	open_info const& info)
{
	OBJECT_ATTRIBUTES object_attributes = {};
	object_attributes.Length = sizeof(object_attributes);
	object_attributes.RootDirectory = root_handle;
	object_attributes.ObjectName = &path;

	LARGE_INTEGER allocation_size;
	allocation_size.QuadPart = 0;

	IO_STATUS_BLOCK io_status_block = make_io_status_block();

	unique_handle handle;
	NTSTATUS status = win32::NtCreateFile(
		vsm::out_resource(handle),
		info.desired_access,
		&object_attributes,
		&io_status_block,
		&allocation_size,
		info.attributes,
		info.share_access,
		info.create_disposition,
		info.create_options,
		/* EaBuffer: */ nullptr,
		/* EaLength: */ 0);

	if (status == STATUS_PENDING)
	{
		status = io_wait(handle.get(), &io_status_block, deadline::never());
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	//TODO: Make sure the synchronous flag is set.
	auto h_flags = handle_flags::none;
	if ((info.create_options & FILE_SYNCHRONOUS_IO_NONALERT) == 0)
	{
		h_flags |= set_file_completion_notification_modes(handle.get());
	}

	return vsm_lazy(handle_with_flags
	{
		.handle = vsm_move(handle),
		.flags = h_flags,
	});
}


#if 0
vsm::result<handle_with_flags> win32::create_file(
	HANDLE const hint_handle,
	file_id_128 const& id,
	open_kind const kind,
	open_args const& args)
{
	vsm_try(info, make_open_info(kind, args));

	info.create_options |= FILE_OPEN_BY_FILE_ID;

	UNICODE_STRING unicode_string;
	unicode_string.Buffer = reinterpret_cast<wchar_t*>(&const_cast<file_id_128&>(id));
	unicode_string.Length = sizeof(id);
	unicode_string.MaximumLength = unicode_string.Length;

	return create_file(hint_handle, unicode_string, info);
}
#endif

vsm::result<handle_with_flags> win32::create_file(
	HANDLE const base_handle,
	any_path_view const path,
	open_info const& info)
{
	kernel_path_storage path_storage;
	vsm_try(kernel_path, make_kernel_path(path_storage,
	{
		.handle = base_handle,
		.path = path,
	}));
	vsm_assert(kernel_path.path.size() <= static_cast<USHORT>(-1) / sizeof(wchar_t));

	UNICODE_STRING unicode_string;
	unicode_string.Buffer = const_cast<wchar_t*>(kernel_path.path.data());
	unicode_string.Length = static_cast<USHORT>(kernel_path.path.size() * sizeof(wchar_t));
	unicode_string.MaximumLength = unicode_string.Length;

	return _create_file(kernel_path.handle, unicode_string, info);
}

vsm::result<handle_with_flags> win32::reopen_file(
	HANDLE const handle,
	open_info const& info)
{
	vsm_assert(handle != NULL); //PRECONDITION

	UNICODE_STRING unicode_string;
	unicode_string.Buffer = nullptr;
	unicode_string.Length = 0;
	unicode_string.MaximumLength = unicode_string.Length;

	return _create_file(handle, unicode_string, info);
}


namespace {

static constexpr size_t file_name_information_buffer_size = 0x7FFF;

static constexpr size_t file_name_information_size =
	sizeof(FILE_NAME_INFORMATION) +
	file_name_information_buffer_size;

struct file_name_information_deleter
{
	void vsm_static_operator_invoke(FILE_NAME_INFORMATION* const information)
	{
		operator delete(static_cast<void*>(information), file_name_information_buffer_size);
	}
};
using file_name_information_ptr = std::unique_ptr<FILE_NAME_INFORMATION, file_name_information_deleter>;

} // namespace

static vsm::result<file_name_information_ptr> allocate_file_name_information()
{
	vsm_try(buffer, allocate_unique(file_name_information_size));
	return vsm_lazy(file_name_information_ptr(new (buffer.release()) FILE_NAME_INFORMATION));
}

static vsm::result<file_name_information_ptr> query_file_name_information(HANDLE const handle)
{
	auto r = allocate_file_name_information();

	if (!r)
	{
		return vsm::unexpected(r.error());
	}

	IO_STATUS_BLOCK io_status_block = make_io_status_block();

	NTSTATUS const status = NtQueryInformationFile(
		handle,
		&io_status_block,
		r->get(),
		file_name_information_size,
		FileNormalizedNameInformation);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return r;
}


static vsm::result<handle_with_flags> open_named_file(open_parameters const& a)
{
	vsm_try(info, open_info::make(a));

	auto const base = a.path.base == native_platform_handle::null
		? NULL
		: unwrap_handle(a.path.base);

	return win32::create_file(base, a.path.path, info);
}

static vsm::result<handle_with_flags> open_anonymous_file(open_parameters const& a)
{
	vsm_try(file, open_unique_file(a));

	// Delete the file by reopening it and
	// setting delete-on-close on the new handle.
	{
		open_info const info =
		{
			.desired_access = SYNCHRONIZE | DELETE,
			.share_access = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			.create_options = FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
		};

		vsm_try(duplicate, win32::reopen_file(file.handle.get(), info));

		FILE_DISPOSITION_INFORMATION_EX information =
		{
			.Flags = FILE_DISPOSITION_DELETE | FILE_DISPOSITION_POSIX_SEMANTICS,
		};

		IO_STATUS_BLOCK io_status_block;
		NTSTATUS const status = NtSetInformationFile(
			duplicate.handle.get(),
			&io_status_block,
			&information,
			sizeof(information),
			FileDispositionInformationEx);

		if (!NT_SUCCESS(status))
		{
			//TODO: Fall back to non-posix semantics or at least
			//      attempt to delete the unique file now.

			return vsm::unexpected(static_cast<kernel_error>(status));
		}
	}

	return file;
}

vsm::result<handle_with_flags> detail::open_file(open_parameters const& a)
{
	switch (a.special & open_options::mode_mask)
	{
	case open_options(0):
		return open_named_file(a);

	case open_options::anonymous:
		return open_anonymous_file(a);

	default:
		return vsm::unexpected(error::invalid_argument);
	}
}

vsm::result<size_t> fs_object_t::get_current_path(
	native_type const& h,
	io_parameters_t<fs_object_t, get_current_path_t> const& a)
{
	vsm_try(information, query_file_name_information(
		unwrap_handle(h.platform_handle)));

	std::wstring_view const wide_path = std::wstring_view(
		information->FileName,
		information->FileNameLength);

	return transcode_string(wide_path, a.buffer);
}
