#include <allio/impl/win32/filesystem_handle.hpp>

#include <allio/win32/nt_error.hpp>

#include <allio/impl/win32/encoding.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/kernel_path.hpp>

using namespace allio;
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

} // namespace

static vsm::result<open_info> make_open_info(filesystem_handle::open_parameters const& args, open_kind const kind)
{
	open_info info =
	{
		.desired_access = SYNCHRONIZE,
		.attributes = 0,
		.share_access = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		.create_disposition = FILE_OPEN,
		.create_options = 0,
	};

	if (args.multiplexable)
	{
		info.handle_flags |= platform_handle::implementation::flags::overlapped;
	}
	else
	{
		info.create_options |= FILE_SYNCHRONOUS_IO_NONALERT;
	}

	switch (args.mode)
	{
	case file_mode::none:
		break;

	case file_mode::read_attributes:
		info.desired_access |= FILE_READ_ATTRIBUTES;
		break;

	case file_mode::write_attributes:
		info.desired_access |= FILE_READ_ATTRIBUTES;
		info.desired_access |= FILE_WRITE_ATTRIBUTES;
		break;

	case file_mode::read:
		info.desired_access |= FILE_READ_ATTRIBUTES;
		info.desired_access |= FILE_READ_DATA;
		break;

	case file_mode::write:
		info.desired_access |= FILE_READ_ATTRIBUTES;
		info.desired_access |= FILE_READ_DATA;
		info.desired_access |= FILE_WRITE_ATTRIBUTES;
		info.desired_access |= FILE_WRITE_DATA;
		info.desired_access |= DELETE;
		break;

	case file_mode::append:
		info.desired_access |= FILE_READ_ATTRIBUTES;
		info.desired_access |= FILE_READ_DATA;
		info.desired_access |= FILE_WRITE_ATTRIBUTES;
		info.desired_access |= FILE_WRITE_DATA;
		info.desired_access |= DELETE;
		info.desired_access |= FILE_APPEND_DATA;
		break;

	default:
		return vsm::unexpected(error::invalid_argument);
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

	switch (kind)
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

	return info;
}

static vsm::result<unique_handle_with_flags> create_file(HANDLE const root_handle, UNICODE_STRING path, open_info const& info)
{
	vsm_try_void(kernel_init());

	OBJECT_ATTRIBUTES object_attributes = {};
	object_attributes.Length = sizeof(object_attributes);
	object_attributes.RootDirectory = root_handle;
	object_attributes.ObjectName = &path;

	LARGE_INTEGER allocation_size;
	allocation_size.QuadPart = 0;

	IO_STATUS_BLOCK io_status_block = make_io_status_block();

	HANDLE handle = INVALID_HANDLE_VALUE;
	NTSTATUS status = win32::NtCreateFile(
		&handle,
		info.desired_access,
		&object_attributes,
		&io_status_block,
		&allocation_size,
		info.attributes,
		info.share_access,
		info.create_disposition,
		info.create_options,
		nullptr,
		0);

	if (status == STATUS_PENDING)
	{
		status = io_wait(handle, &io_status_block, deadline());
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(nt_error(status));
	}

	handle_flags handle_flags = info.handle_flags;
	if ((info.create_options & FILE_SYNCHRONOUS_IO_NONALERT) == 0)
	{
		handle_flags |= set_multiplexable_completion_modes(handle);
	}

	return vsm::result<unique_handle_with_flags>(vsm::result_value, handle, handle_flags);
}


vsm::result<unique_handle_with_flags> win32::create_file(
	filesystem_handle const* const hint, file_id_128 const& id,
	filesystem_handle::open_parameters const& args, open_kind const kind)
{
	vsm_try((auto, info), make_open_info(args, kind));

	info.create_options |= FILE_OPEN_BY_FILE_ID;

	HANDLE const hint_handle =
		hint != nullptr && *hint
			? unwrap_handle(hint->get_platform_handle())
			: NULL;

	UNICODE_STRING unicode_string;
	unicode_string.Buffer = (wchar_t*)&id;
	unicode_string.Length = sizeof(id);
	unicode_string.MaximumLength = unicode_string.Length;

	return create_file(hint_handle, unicode_string, info);
}

vsm::result<unique_handle_with_flags> win32::create_file(
	filesystem_handle const* const base, input_path_view const path,
	filesystem_handle::open_parameters const& args, open_kind const kind)
{
	vsm_try(info, make_open_info(args, kind));

	HANDLE const base_handle =
		base != nullptr && *base
			? unwrap_handle(base->get_platform_handle())
			: NULL;

	kernel_path_storage path_storage;
	vsm_try(kernel_path, make_kernel_path(path_storage,
	{
		.handle = base_handle,
		.path = path
	}));
	vsm_assert(kernel_path.path.size() <= static_cast<USHORT>(-1) / sizeof(wchar_t));

	UNICODE_STRING unicode_string;
	unicode_string.Buffer = const_cast<wchar_t*>(kernel_path.path.data());
	unicode_string.Length = static_cast<USHORT>(kernel_path.path.size() * sizeof(wchar_t));
	unicode_string.MaximumLength = unicode_string.Length;

	return create_file(base_handle, unicode_string, info);
}

vsm::result<unique_handle_with_flags> win32::reopen_file(
	filesystem_handle const& base,
	filesystem_handle::open_parameters const& args, open_kind const kind)
{
	vsm_assert(base);

	vsm_try(info, make_open_info(args, kind));

	UNICODE_STRING unicode_string;
	unicode_string.Buffer = nullptr;
	unicode_string.Length = 0;
	unicode_string.MaximumLength = unicode_string.Length;

	return create_file(unwrap_handle(base.get_platform_handle()), unicode_string, info);
}


namespace {

static constexpr size_t file_name_information_buffer_size = 0x7FFF;

static constexpr size_t file_name_information_size =
	sizeof(FILE_NAME_INFORMATION) + file_name_information_buffer_size;

struct file_name_information_deleter
{
	void operator()(FILE_NAME_INFORMATION* const information) const
	{
		operator delete(static_cast<void*>(information), file_name_information_buffer_size);
	}
};
using file_name_information_ptr = std::unique_ptr<FILE_NAME_INFORMATION, file_name_information_deleter>;

static vsm::result<file_name_information_ptr> allocate_file_name_information()
{
	void* const block = operator new(file_name_information_size, std::nothrow);
	if (block == nullptr)
	{
		return vsm::unexpected(error::not_enough_memory);
	}
	return vsm::result<file_name_information_ptr>(vsm::result_value, new (block) FILE_NAME_INFORMATION);
}

static vsm::result<file_name_information_ptr> query_file_name_information(HANDLE const handle)
{
	auto r = allocate_file_name_information();

	if (!r)
	{
		return vsm::unexpected(r.error());
	}

	IO_STATUS_BLOCK io_status_block = make_io_status_block();

	NTSTATUS status = NtQueryInformationFile(
		handle,
		&io_status_block,
		r->get(),
		file_name_information_size,
		FileNormalizedNameInformation);

	if (status == STATUS_PENDING)
	{
		status = io_wait(handle, &io_status_block, deadline::never());
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<nt_error>(status));
	}

	return r;
}

} // namespace


vsm::result<void> filesystem_handle::sync_impl(io::parameters_with_result<io::get_current_path> const& args)
{
	filesystem_handle const& h = *args.handle;

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	vsm_try(information, query_file_name_information(unwrap_handle(h.get_platform_handle())));
	std::wstring_view const wide_path = std::wstring_view(information->FileName, information->FileNameLength);
	return args.produce(copy_string(wide_path, args.output));
}
