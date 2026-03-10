#include <allio/impl/win32/handles/fs_object.hpp>

#include <allio/detail/config.hpp>
#include <allio/impl/new.hpp>
#include <allio/impl/storage_provider.hpp>
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

vsm::result<platform_open_options> platform_open_options::make(generic_open_options const& options)
{
	vsm_assert(options.opening != file_opening(0));

	platform_open_options r =
	{
		// TODO: Is synchronize needed for path_handle?
		.desired_access = SYNCHRONIZE,
	};

	if (vsm::any_flags(options.flags, io_flags::create_synchronous))
	{
		r.create_options |= FILE_SYNCHRONOUS_IO_NONALERT;
	}

	switch (options.kind)
	{
	case open_kind::path:
		if (options.mode != file_mode::none)
		{
			// When opening a path, it is not possible to specify a mode.
			return vsm::unexpected(allio_error(error::invalid_argument));
		}
		break;

	case open_kind::file:
		r.create_options |= FILE_NON_DIRECTORY_FILE;
		break;

	case open_kind::directory:
		r.create_options |= FILE_DIRECTORY_FILE;
		break;
	}

	if (options.mode != file_mode::none)
	{
		r.desired_access |= READ_CONTROL;
	}
	if (vsm::any_flags(options.mode, file_mode::read_data))
	{
		r.desired_access |= FILE_GENERIC_READ;
	}
	if (vsm::any_flags(options.mode, file_mode::write_data))
	{
		r.desired_access |= FILE_GENERIC_WRITE | DELETE;
	}
	if (vsm::any_flags(options.mode, file_mode::read_attributes))
	{
		r.desired_access |= FILE_READ_ATTRIBUTES | FILE_READ_EA;
	}
	if (vsm::any_flags(options.mode, file_mode::write_attributes))
	{
		r.desired_access |= FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA;
	}

	// Disposition
	switch (options.opening)
	{
	case file_opening::open_existing:
		r.create_disposition = FILE_OPEN;
		break;

	case file_opening::create_only:
		r.create_disposition = FILE_CREATE;
		break;

	case file_opening::open_or_create:
		r.create_disposition = FILE_OPEN_IF;
		break;

	case file_opening::truncate_existing:
		r.create_disposition = FILE_OVERWRITE;
		break;

	case file_opening::replace_existing:
		r.create_disposition = FILE_SUPERSEDE;
		break;
	}

	// Sharing
	if (vsm::any_flags(options.sharing, file_sharing::unlink))
	{
		r.share_access |= FILE_SHARE_DELETE;
	}
	if (vsm::any_flags(options.sharing, file_sharing::read))
	{
		r.share_access |= FILE_SHARE_READ;
	}
	if (vsm::any_flags(options.sharing, file_sharing::write))
	{
		r.share_access |= FILE_SHARE_WRITE;
	}

	return r;
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
	platform_open_options const& options)
{
	OBJECT_ATTRIBUTES object_attributes = {};
	object_attributes.Length = sizeof(object_attributes);
	object_attributes.RootDirectory = base_handle;
	object_attributes.ObjectName = &path;
	object_attributes.Attributes = options.object_attributes;

	LARGE_INTEGER allocation_size;
	allocation_size.QuadPart = 0;

	IO_STATUS_BLOCK io_status_block;

	unique_handle handle;
	NTSTATUS const status = win32::NtCreateFile(
		vsm::out_resource(handle),
		options.desired_access,
		&object_attributes,
		&io_status_block,
		&allocation_size,
		options.attributes,
		options.share_access,
		options.create_disposition,
		options.create_options,
		/* EaBuffer: */ nullptr,
		/* EaLength: */ 0);
	vsm_assert(status != STATUS_PENDING);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

	auto h_flags = handle_flags::none;
	if ((options.create_options & FILE_SYNCHRONOUS_IO_NONALERT) == 0)
	{
		h_flags |= platform_object_t::impl_type::flags::overlapped;
		h_flags |= set_file_completion_notification_modes(handle.get());
	}

	return vsm_lazy(handle_with_flags
	{
		.handle = vsm_move(handle),
		.flags = h_flags,
	});
}

vsm::result<handle_with_flags> win32::reopen_file(
	HANDLE const handle,
	platform_open_options const& options)
{
	vsm_assert(handle != NULL); //PRECONDITION

	return create_file(handle, make_unicode_string(), options);
}

static constexpr size_t file_link_information_size(size_t const path_size)
{
	return sizeof(FILE_LINK_INFORMATION) + (path_size - 1) * sizeof(wchar_t);
}

vsm::result<void> win32::_link_file_at(
	HANDLE const handle,
	HANDLE const base_handle,
	std::wstring_view const path,
	bool const replace_existing_file)
{
#if 0
	// TODO: Just debugging
	{
		HANDLE delete_handle = handle;

#if 0
		open_info const dup_info =
		{
			.desired_access = SYNCHRONIZE | DELETE,
			.share_access = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			.create_options = FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
		};

		vsm_try(duplicate, win32::reopen_file(handle, dup_info));

		delete_handle = duplicate.handle.get();
#endif

		FILE_DISPOSITION_INFORMATION_EX information2 =
		{
			.Flags = FILE_DISPOSITION_DELETE /*| FILE_DISPOSITION_POSIX_SEMANTICS*/,
		};

		IO_STATUS_BLOCK io_status_block2;
		[[maybe_unused]] NTSTATUS const status2 = NtSetInformationFile(
			delete_handle,
			&io_status_block2,
			&information2,
			sizeof(information2),
			FileDispositionInformationEx);

		[[maybe_unused]] int x = 0;
	}

	// TODO: Just debugging
	{
#if 1
		HANDLE delete_handle = handle;

		FILE_DISPOSITION_INFORMATION_EX information2 =
		{
			.Flags = FILE_DISPOSITION_ON_CLOSE,
		};

		IO_STATUS_BLOCK io_status_block2;
		[[maybe_unused]] NTSTATUS const status2 = NtSetInformationFile(
			delete_handle,
			&io_status_block2,
			&information2,
			sizeof(information2),
			FileDispositionInformationEx);
#endif

		[[maybe_unused]] int x = 0;
	}
#endif

	size_t const information_size = file_link_information_size(path.size());

	dynamic_storage_provider<file_link_information_size(MAX_PATH)> storage_provider;
	vsm_try(storage, storage_provider.get_storage(
		information_size,
		std::align_val_t(alignof(FILE_LINK_INFORMATION))));

	auto const information = ::new (storage) FILE_LINK_INFORMATION
	{
		.ReplaceIfExists = replace_existing_file,
		.RootDirectory = base_handle,
		.FileNameLength = vsm::truncating(path.size() * sizeof(wchar_t)),
	};
	std::memcpy(information->FileName, path.data(), path.size() * sizeof(wchar_t));

	IO_STATUS_BLOCK io_status_block;
	NTSTATUS const status = NtSetInformationFile(
		handle,
		&io_status_block,
		information,
		vsm::truncating(information_size),
		FileLinkInformation);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

	return {};
}

vsm::result<void> win32::link_file_at(
	HANDLE const handle,
	HANDLE const base_handle,
	any_path_view const path,
	bool const replace_existing_file)
{
	kernel_path_storage path_storage;
	vsm_try(kernel_path, make_kernel_path(path_storage,
	{
		.handle = base_handle,
		.path = path,
	}));

	return _link_file_at(handle, kernel_path.handle, kernel_path.path, replace_existing_file);
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
		allio_release_storage(
			static_cast<void*>(information),
			file_name_information_buffer_size,
			new_alignment_for<void>,
			allio_allocation_strategy_generic);
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
	vsm_assert(std::has_single_bit(std::to_underlying(kind))); //PRECONDITION


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
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
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
			allio_error(e),
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
		return vsm::unexpected(allio_error(error::invalid_argument));
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

	return vsm::unexpected(allio_error(error::unrepresentable_path));
}


static vsm::result<void> delete_file(HANDLE const handle, bool try_posix_semantics)
{
	while (true)
	{
		FILE_DISPOSITION_INFORMATION_EX information =
		{
			.Flags = FILE_DISPOSITION_DELETE,
		};

		if (try_posix_semantics)
		{
			information.Flags |= FILE_DISPOSITION_POSIX_SEMANTICS;
		}

		IO_STATUS_BLOCK io_status_block;
		NTSTATUS const status = NtSetInformationFile(
			handle,
			&io_status_block,
			&information,
			sizeof(information),
			FileDispositionInformationEx);

		if (NT_SUCCESS(status))
		{
			return {};
		}

		if (!try_posix_semantics || status != STATUS_INVALID_PARAMETER)
		{
			return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
		}

		try_posix_semantics = false;
	}
}

[[maybe_unused]] // TODO: Not currently used
static vsm::result<void> reopen_and_delete_file(HANDLE const handle, bool try_posix_semantics)
{
	platform_open_options const duplicate_options =
	{
		.desired_access = SYNCHRONIZE | DELETE,
		.share_access = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		.create_options = FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
	};

	vsm_try(duplicate, win32::reopen_file(handle, duplicate_options));
	return delete_file(duplicate.handle.get(), try_posix_semantics);
}


vsm::result<unique_handle> detail::open_path_base(
	platform_handle_type const base,
	any_path_view const path)
{
	platform_open_options const options =
	{
		.desired_access = SYNCHRONIZE,
		.share_access = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		.create_disposition = FILE_OPEN,
		.create_options = FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
	};

	vsm_try_bind((handle, flags), detail::open_file(base, path, options));

	return vsm_move(handle);
}

vsm::result<handle_with_flags> detail::open_file(
	platform_handle_type const base,
	any_path_view const path,
	platform_open_options const& options)
{
	kernel_path_storage path_storage;
	vsm_try(kernel_path, make_kernel_path(path_storage,
	{
		.handle = base,
		.path = path,
	}));

	return win32::create_file(kernel_path.handle, make_unicode_string(kernel_path.path), options);
}

vsm::result<handle_with_flags> detail::open_anonymous_file(
	platform_handle_type const base,
	platform_open_options const& options)
{
	platform_open_options unique_options = options;
	unique_options.create_options |= FILE_DELETE_ON_CLOSE;

	vsm_try(file, detail::open_unique_file(base, unique_options));

	// TODO: Delete (with posix semantics) if intended for temporary usage.

#if 0
	if (vsm::any_flags(local_a.special, open_options::temporary))
	{
		vsm_try_void(reopen_and_delete_file(
			file.handle.get(),
			/* try_posix_semantics: */ true));
	}
#endif

	return file;
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

	return copy_or_transcode_string(wide_path, a.buffer.string());
}

vsm::result<void> fs_object_t::link_at(
	native_handle<fs_object_t> const& h,
	io_parameters_t<fs_object_t, link_at_t> const& a)
{
	HANDLE const base_handle = a.path.base == nullptr
		? NULL
		: unwrap_handle(a.path.base->platform_handle);

	return win32::link_file_at(
		unwrap_handle(h.platform_handle),
		base_handle,
		a.path.path,
		a.replace_existing_file);
}


//TODO: Why is this inside a namespace?
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
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
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
