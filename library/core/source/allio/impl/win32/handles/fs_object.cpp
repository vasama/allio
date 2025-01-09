#include <allio/impl/win32/handles/fs_object.hpp>

#include <allio/detail/config.hpp>
#include <allio/impl/new.hpp>
#include <allio/impl/transcode.hpp>
#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/kernel_path.hpp>
#include <allio/win32/kernel_error.hpp>

#include <vsm/lazy.hpp>
#include <vsm/numeric.hpp>
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

	if (vsm::no_flags(args.flags, io_flags::create_non_blocking))
	{
		info.create_options |= FILE_SYNCHRONOUS_IO_NONALERT;
	}

	switch (args.special & open_kind::mask)
	{
		vsm_msvc_warning(push)

		// Disable C4063: Case is not a valid value for switch of enum.
		vsm_msvc_warning(disable: 4063)

	case open_kind::path:
		if (args.mode != file_mode::none)
		{
			return vsm::unexpected(error::invalid_argument);
		}
		break;

	case open_kind::file:
		info.create_options |= FILE_NON_DIRECTORY_FILE;
		break;

	case open_kind::directory:
		info.create_options |= FILE_DIRECTORY_FILE;
		break;

		vsm_msvc_warning(pop)

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
	UNICODE_STRING path,
	open_info const& info)
{
	OBJECT_ATTRIBUTES object_attributes = {};
	object_attributes.Length = sizeof(object_attributes);
	object_attributes.RootDirectory = base_handle;
	object_attributes.ObjectName = &path;

	LARGE_INTEGER allocation_size;
	allocation_size.QuadPart = 0;

	IO_STATUS_BLOCK io_status_block;

	unique_handle handle;
	NTSTATUS const status = win32::NtCreateFile(
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
	vsm_assert(status != STATUS_PENDING);

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

	return create_file(kernel_path.handle, make_unicode_string(kernel_path.path), info);
}

vsm::result<handle_with_flags> win32::reopen_file(
	HANDLE const handle,
	open_info const& info)
{
	vsm_assert(handle != NULL); //PRECONDITION

	return create_file(handle, make_unicode_string(), info);
}


namespace {

// 0x7FFF for the maximum NT path length +1 for the Win32 null terminator.
static constexpr uint32_t file_name_information_buffer_size = 0x8000;

static constexpr size_t file_name_information_size =
	offsetof(FILE_NAME_INFORMATION, FileName) +
	file_name_information_buffer_size;

struct file_name_information_deleter
{
	vsm_static_operator void operator()(
		FILE_NAME_INFORMATION* const information) vsm_static_operator_const
	{
		release_storage(
			static_cast<void*>(information),
			file_name_information_buffer_size,
			alignof(FILE_NAME_INFORMATION),
			/* automatic: */ false);
	}
};

using file_name_information_ptr = std::unique_ptr<
	FILE_NAME_INFORMATION,
	file_name_information_deleter>;

} // namespace

static vsm::result<file_name_information_ptr> allocate_file_name_information()
{
	vsm_try(buffer, allocate_unique(file_name_information_size));

	return vsm::result<file_name_information_ptr>(
		vsm::result_value,
		::new (buffer.release()) FILE_NAME_INFORMATION);
}

namespace {

template<typename Info>
struct error_code_with_info : std::error_code
{
	Info info;

	using std::error_code::error_code;

	explicit error_code_with_info(std::error_code const code, Info const& info)
		: std::error_code(code)
		, info(info)
	{
	}

	[[nodiscard]] std::error_code const& discard_info() const
	{
		return static_cast<std::error_code const&>(*this);
	}
};

} // namespace

static vsm::result<void, error_code_with_info<bool>> query_file_name_information(
	HANDLE const handle,
	path_kind const kind,
	FILE_NAME_INFORMATION* const information)
{
	vsm_assert(std::popcount(std::to_underlying(kind)) == 1); //PRECONDITION


	DWORD flags = FILE_NAME_NORMALIZED;

	switch (kind)
	{
	case path_kind::windows_nt:
		flags |= VOLUME_NAME_NT;
		break;

	case path_kind::windows_volume_guid:
		flags |= VOLUME_NAME_GUID;
		break;

	case path_kind::windows_dos:
		flags |= VOLUME_NAME_DOS;
		break;

	default:
	case path_kind::any:
		vsm_unreachable();
	}

#if allio_config_ntapi == allio_ntapi_always
	//TODO: Implement this properly:

	IO_STATUS_BLOCK io_status_block;

	NTSTATUS const status = NtQueryInformationFile(
		handle,
		&io_status_block,
		information,
		file_name_information_size,
		FileNormalizedNameInformation);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}
#else
	DWORD const name_size = GetFinalPathNameByHandleW(
		handle,
		information->FileName,
		file_name_information_buffer_size,
		flags);

	if (name_size == 0 || name_size > file_name_information_buffer_size)
	{
		system_error const e = get_last_error();
		return vsm::unexpected(error_code_with_info<bool>(
			e,
			e == static_cast<system_error>(ERROR_PATH_NOT_FOUND)));
	}

	information->FileNameLength = name_size * sizeof(wchar_t);
#endif

	if (kind == path_kind::windows_nt)
	{
		static constexpr std::wstring_view root = L"\\??\\GLOBALROOT";
		static constexpr size_t root_size = root.size() * sizeof(wchar_t);

		size_t const required_size = root_size + information->FileNameLength;

		if (required_size > file_name_information_buffer_size)
		{
			return vsm::unexpected(error_code_with_info<bool>(error::filename_too_long));
		}

		std::memmove(
			information->FileName + root_size / sizeof(wchar_t),
			information->FileName,
			information->FileNameLength);

		std::memcpy(information->FileName, root.data(), root_size);

		information->FileNameLength = vsm::truncating(required_size);
	}

	return {};
}

static vsm::result<file_name_information_ptr> query_file_name_information(
	HANDLE const handle,
	path_kind const kind)
{
	using path_kind_type = std::underlying_type_t<path_kind>;

	// The path_kind flags must start at 1.
	static_assert(std::to_underlying(path_kind::any) & 1);

	static constexpr path_kind_type flag_bound = std::bit_ceil(std::to_underlying(path_kind::any));


	if (kind == static_cast<path_kind>(0))
	{
		return vsm::unexpected(error::invalid_argument);
	}

	vsm_try(name_information, allocate_file_name_information());

	// Iterate over each set flag and attempt to get such a path for the file.
	for (path_kind_type flag_value = 1; flag_value < flag_bound; flag_value <<= 1)
	{
		auto const flag = vsm::to_enum<path_kind>(flag_value);

		if (vsm::no_flags(kind, flag))
		{
			continue;
		}

		auto const r = query_file_name_information(
			handle,
			flag,
			name_information.get());

		if (r)
		{
			return name_information;
		}

		if (!r.error().info)
		{
			// Propagate any error except those caused by the file having a non-representable path.
			// That could be because a DOS path was requested, but the file is on a volume that is
			// not currently mounted with a drive letter, or because a GUID path was requested, but
			// the file is on a network volume with no associated GUID.
			return vsm::unexpected(r.error());
		}
	}

	return vsm::unexpected(error::unrepresentable_path);
}


static vsm::result<handle_with_flags> open_named_file(open_parameters const& a)
{
	vsm_try(info, open_info::make(a));

	auto const base = a.path.base == nullptr
		? NULL
		: unwrap_handle(a.path.base->platform_handle);

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
	if (vsm::any_flags(a.special, open_options::anonymous))
	{
		return open_anonymous_file(a);
	}
	else
	{
		return open_named_file(a);
	}
}

vsm::result<size_t> fs_object_t::get_current_path(
	native_handle<fs_object_t> const& h,
	io_parameters_t<fs_object_t, get_current_path_t> const& a)
{
	vsm_try(information, query_file_name_information(
		unwrap_handle(h.platform_handle),
		a.kind));

	vsm_assert(information->FileNameLength % sizeof(wchar_t) == 0);
	std::wstring_view const wide_path = std::wstring_view(
		information->FileName,
		information->FileNameLength / sizeof(wchar_t));

	return transcode_string(wide_path, a.buffer);
}



namespace allio::win32 {

static vsm::result<FILE_ID_INFO> get_file_id_info(HANDLE const handle)
{
	FILE_ID_INFO information;

	IO_STATUS_BLOCK io_status_block;

	NTSTATUS const status = NtQueryInformationFile(
		handle,
		&io_status_block,
		&information,
		sizeof(information),
		FileIdInformation);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return information;
}

} // namespace allio::win32

vsm::result<bool> detail::_equivalent(
	native_handle<fs_object_t> const* const lhs,
	native_handle<fs_object_t> const* const rhs)
{
	vsm_try(lhs_id, win32::get_file_id_info(unwrap_handle(lhs->platform_handle)));
	vsm_try(rhs_id, win32::get_file_id_info(unwrap_handle(rhs->platform_handle)));

	return
		lhs_id.VolumeSerialNumber == rhs_id.VolumeSerialNumber &&
		std::memcmp(&lhs_id.FileId, &rhs_id.FileId, sizeof(FILE_ID_128)) == 0;
}
