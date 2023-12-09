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

namespace {

struct open_info
{
	handle_flags handle_flags;
	ACCESS_MASK desired_access;
	DWORD attributes;
	DWORD share_access;
	DWORD create_disposition;
	DWORD create_options;
};

static vsm::result<open_info> make_open_info(open_parameters const& args)
{
	open_info info =
	{
		.desired_access = SYNCHRONIZE,
	};

	switch (args.kind)
	{
	case open_kind::file:
		info.create_options |= FILE_NON_DIRECTORY_FILE;
		break;

	case open_kind::directory:
		info.create_options |= FILE_DIRECTORY_FILE;
		break;

	default:
		vsm_assert(false);
	}
	
	if (args.mode != file_mode::none)
	{
		info.desired_access |= READ_CONTROL;
	}
	if (vsm::any_flags(args.mode, file_mode::read_data))
	{
		info.desired_access |= FILE_GENERIC_READ;
		info.handle_flags |= fs_object_t::flags::readable;
	}
	if (vsm::any_flags(args.mode, file_mode::write_data))
	{
		info.desired_access |= FILE_GENERIC_WRITE | DELETE;
		info.handle_flags |= fs_object_t::flags::writable;
	}
	if (vsm::any_flags(args.mode, file_mode::read_attributes))
	{
		info.desired_access |= FILE_READ_ATTRIBUTES | FILE_READ_EA;
	}
	if (vsm::any_flags(args.mode, file_mode::write_attributes))
	{
		info.desired_access |= FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA;
	}

	switch (args.creation)
	{
	case file_creation::open_existing:
		info.create_disposition = FILE_OPEN;
		break;

	case file_creation::create_only:
		info.create_disposition = FILE_CREATE;
		break;

	case file_creation::open_or_create:
		info.create_disposition = FILE_OPEN_IF;
		break;

	case file_creation::truncate_existing:
		info.create_disposition = FILE_OVERWRITE;
		break;

	case file_creation::replace_existing:
		info.create_disposition = FILE_SUPERSEDE;
		break;
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

	if (!args.multiplexable)
	{
		info.create_options |= FILE_SYNCHRONOUS_IO_NONALERT;
		info.handle_flags |= platform_object_t::impl_type::flags::synchronous;
	}

	return info;
}

} // namespace

static vsm::result<unique_handle_with_flags> create_file(
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

	handle_flags flags = info.handle_flags;

	if ((info.create_options & FILE_SYNCHRONOUS_IO_NONALERT) == 0)
	{
		flags |= set_file_completion_notification_modes(handle.get());
	}

	return vsm_lazy(unique_handle_with_flags
	{
		.handle = vsm_move(handle),
		.flags = flags,
	});
}


#if 0
vsm::result<unique_handle_with_flags> win32::create_file(
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

open_parameters open_parameters::make(
	open_kind const kind,
	fs_io::open_t::optional_params_type const& args)
{
	file_mode default_mode = file_mode::none;
	switch (kind)
	{
	case open_kind::file:
		default_mode = file_mode::read_write;
		break;

	case open_kind::directory:
		default_mode = file_mode::read;
		break;
	}

	file_creation default_creation = file_creation::open_existing;
	if (vsm::any_flags(default_mode, file_mode::write_data))
	{
		default_creation = file_creation::open_or_create;
	}

	return open_parameters
	{
		.kind = kind,
		.inheritable = args.inheritable,
		.mode = args.mode.value_or(default_mode),
		.creation = args.creation.value_or(default_creation),
		.sharing = args.sharing,
		.caching = args.caching,
		.flags = args.flags,
	};
}

vsm::result<unique_handle_with_flags> win32::create_file(
	HANDLE const base_handle,
	any_path_view const path,
	open_parameters const& args)
{
	vsm_try(info, make_open_info(args));

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

	return create_file(kernel_path.handle, unicode_string, info);
}

vsm::result<unique_handle_with_flags> win32::reopen_file(
	HANDLE const handle,
	open_parameters const& args)
{
	vsm_assert(handle != NULL); //PRECONDITION

	vsm_try(info, make_open_info(args));

	UNICODE_STRING unicode_string;
	unicode_string.Buffer = nullptr;
	unicode_string.Length = 0;
	unicode_string.MaximumLength = unicode_string.Length;

	return create_file(handle, unicode_string, info);
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
