#include <allio/impl/win32/filesystem.hpp>

#include <allio/any_string.hpp>
#include <allio/detail/config.hpp>
#include <allio/detail/platform.hpp>
#include <allio/detail/unique_handle.hpp>
#include <allio/impl/transcode.hpp>
#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel_path.hpp>
#include <allio/win32/path_format.hpp>

#include <vsm/lift.hpp>
#include <vsm/numeric.hpp>
#include <vsm/out_resource.hpp>

#include <ranges>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

template<typename Char>
static bool _is_null_device_path(std::basic_string_view<Char> const path)
{
	auto const to_upper_or_zero = [](char32_t character)
	{
		if ('a' <= character && character <= 'z')
		{
			character = 'A' + (character - 'a');
		}
	
		return character >= 0x100
			? 0
			: static_cast<uint8_t>(character);
	};

	auto const test = [&](std::string_view const null_path)
	{
		return std::ranges::equal(
			null_path,
			std::views::transform(path, to_upper_or_zero));
	};

	return test("NUL") || test("\\??\\DEVICE\\NULL");
}

static bool _is_null_device_path(string_length_out_of_range_t)
{
	return false;
}

bool detail::is_null_device_path(any_path_view const path)
{
	return path.string().visit(vsm_lift(_is_null_device_path));
}


vsm::result<unique_handle> win32::create_file(
	HANDLE const base_handle,
	std::wstring_view const path,
	platform_open_options const& options)
{
	auto unicode_path = make_unicode_string(path);

	OBJECT_ATTRIBUTES object_attributes = {};
	object_attributes.Length = sizeof(object_attributes);
	object_attributes.RootDirectory = base_handle;
	object_attributes.ObjectName = &unicode_path;
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

	return handle;
}

vsm::result<unique_handle> win32::reopen_file(
	HANDLE const handle,
	platform_open_options const& options)
{
	vsm_assert(handle != NULL); //PRECONDITION

	return create_file(handle, std::wstring_view(), options);
}


vsm::result<unique_handle> win32::open_anchor_file(
	HANDLE const base_handle,
	std::wstring_view const path,
	bool const file)
{
	platform_open_options options =
	{
		.desired_access = SYNCHRONIZE,
		.share_access = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		.create_disposition = FILE_OPEN,
		.create_options = FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
	};

	if (file)
	{
		options.create_options |= FILE_NON_DIRECTORY_FILE;
	}

	return create_file(base_handle, path, options);
}

vsm::result<unique_handle> detail::open_anchor_file(
	HANDLE const base_handle,
	any_path_view const path,
	bool const may_open_leaf)
{
	kernel_path_storage path_storage;
	kernel_path_handle path_handle;

	vsm_try(kernel_path, make_kernel_path(
		path_storage,
		base_handle,
		path,
		&path_handle));

	return win32::open_anchor_file(path_handle.get(), kernel_path, may_open_leaf);
}


vsm::result<void> win32::link_file_at(
	HANDLE const handle,
	HANDLE const base_handle,
	std::wstring_view const path,
	bool const replace_existing_file)
{
	static constexpr auto get_information_size = [&](size_t const path_size)
	{
		return offsetof(FILE_LINK_INFORMATION, FileName) + path_size * sizeof(wchar_t);
	};

	untyped_storage<get_information_size(MAX_PATH)> information_storage;
	size_t const information_size = get_information_size(path.size());

	vsm_try(storage, information_storage.resize(information_size, resize_mode::discard));
	auto const information = ::new (storage.data()) FILE_LINK_INFORMATION
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

vsm::result<void> detail::link_file_at(
	HANDLE const handle,
	HANDLE const base_handle,
	any_path_view const path,
	bool const replace_existing_file)
{
	kernel_path_storage path_storage;
	kernel_path_handle path_handle;

	vsm_try(kernel_path, make_kernel_path(
		path_storage,
		base_handle,
		path,
		&path_handle));

	HANDLE kernel_path_base = base_handle;
	unique_handle local_kernel_path_base;

	if (path_handle.get() != NULL)
	{
		vsm_try_assign(local_kernel_path_base, path_handle.duplicate_and_close());
		kernel_path_base = local_kernel_path_base.get();
	}

	return win32::link_file_at(
		handle,
		kernel_path_base,
		kernel_path,
		replace_existing_file);
}


namespace {

using name_information_storage_type = untyped_storage<16 + MAX_PATH * sizeof(wchar_t)>;

static vsm::result<std::wstring_view> query_object_path(
	untyped_storage_base& storage,
	HANDLE const handle)
{
	while (true)
	{
		auto const information = reinterpret_cast<OBJECT_NAME_INFORMATION*>(storage.data());

		ULONG information_size;
		NTSTATUS const status = win32::NtQueryObject(
			handle,
			ObjectNameInformation,
			storage.data(),
			vsm::truncating(storage.size()),
			&information_size);

		if (NT_SUCCESS(status))
		{
			return from_unicode_string(information->Name);
		}

		if (status != STATUS_BUFFER_OVERFLOW)
		{
			return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
		}

		vsm_try_discard(storage.resize(information_size, resize_mode::discard));
	}
}

static vsm::result<std::wstring_view> get_device_name(std::wstring_view object_path)
{
	constexpr std::wstring_view device_directory = L"\\Device\\";

	if (object_path.starts_with(device_directory))
	{
		size_t const device_name_end = object_path.find('\\', device_directory.size());

		if (device_name_end != std::wstring_view::npos)
		{
			return object_path.substr(0, device_name_end);
		}
	}

	return vsm::unexpected(allio_error(error::unsupported_operation));
}

static vsm::result<std::wstring_view> query_normalized_file_path(
	untyped_storage_base& storage,
	HANDLE const handle)
{
	while (true)
	{
		auto const information = reinterpret_cast<FILE_NAME_INFORMATION*>(storage.data());

		IO_STATUS_BLOCK io_status_block;
		NTSTATUS const status = NtQueryInformationFile(
			handle,
			&io_status_block,
			information,
			vsm::truncating(storage.size()),
			FileNormalizedNameInformation);

		if (NT_SUCCESS(status))
		{
			return std::wstring_view(
				information->FileName,
				information->FileNameLength / sizeof(wchar_t));
		}

		if (status != STATUS_BUFFER_OVERFLOW)
		{
			return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
		}

		static constexpr size_t offset = offsetof(FILE_NAME_INFORMATION, FileName);
		vsm_try_discard(storage.resize(offset + information->FileNameLength, resize_mode::discard));
	}
}


static vsm::result<HANDLE> open_mount_manager()
{
	static constexpr auto open = []() -> vsm::result<HANDLE>
	{
		HANDLE handle;

		UNICODE_STRING name = make_unicode_string(L"\\??\\MountPointManager");

		OBJECT_ATTRIBUTES object_attributes =
		{
			.Length = sizeof(OBJECT_ATTRIBUTES),
			.ObjectName = &name,
			.Attributes = OBJ_OPENIF,
		};

		IO_STATUS_BLOCK io_status_block;
		NTSTATUS const status = win32::NtCreateFile(
			&handle,
			SYNCHRONIZE | FILE_READ_ATTRIBUTES,
			&object_attributes,
			&io_status_block,
			/* AllocationSize: */ 0,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
			/* EaBuffer: */ nullptr,
			/* EaLength: */ 0);

		if (!NT_SUCCESS(status))
		{
			return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
		}

		return handle;
	};

	static vsm::result<HANDLE> const r = open();
	return r;
}


#if 0
static vsm::result<std::wstring_view> query_dos_volume_name(
	untyped_storage_base& storage,
	MOUNTDEV_NAME* const name)
{
	vsm_try(mount_manager, open_mount_manager());

	while (true)
	{
		auto const paths = reinterpret_cast<MOUNTMGR_VOLUME_PATHS*>(storage.data());

		IO_STATUS_BLOCK io_status_block;
		NTSTATUS const status = win32::NtDeviceIoControlFile(
			mount_manager,
			/* Event: */ NULL,
			/* ApcRoutine: */ nullptr,
			/* ApcContext: */ nullptr,
			&io_status_block,
			IOCTL_MOUNTMGR_QUERY_DOS_VOLUME_PATH,
			name,
			offsetof(MOUNTDEV_NAME, Name) + name->NameLength,
			paths,
			vsm::truncating(storage.size()));

		if (NT_SUCCESS(status))
		{
			return std::wstring_view(paths->MultiSz);
		}

		if (status != STATUS_BUFFER_OVERFLOW)
		{
			return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
		}

		static constexpr size_t offset = offsetof(MOUNTMGR_VOLUME_PATHS, MultiSz);
		vsm_try_discard(storage.resize(offset + paths->MultiSzLength, resize_mode::discard));
	}
}
#endif


struct mount_point_name
{
	MOUNTMGR_MOUNT_POINT* mount_point;
	size_t mount_point_size;
};

static vsm::result<mount_point_name> get_mount_point_name(
	untyped_storage_base& storage,
	std::wstring_view const device_name)
{
	size_t const size = sizeof(MOUNTMGR_MOUNT_POINT) + device_name.size() * sizeof(wchar_t);
	vsm_try_discard(storage.resize(size, resize_mode::discard));

	auto const mount_point = ::new (storage.data()) MOUNTMGR_MOUNT_POINT
	{
		.DeviceNameOffset = sizeof(MOUNTMGR_MOUNT_POINT),
		.DeviceNameLength = vsm::truncating(device_name.size() * sizeof(wchar_t)),
	};

	std::memcpy(mount_point + 1, device_name.data(), device_name.size() * sizeof(wchar_t));

	return mount_point_name{ mount_point, size };
}


class mount_point_view_base
{
protected:
	MOUNTMGR_MOUNT_POINTS const* m_mount_points;

	explicit mount_point_view_base(MOUNTMGR_MOUNT_POINTS const* const mount_points)
		: m_mount_points(mount_points)
	{
	}

	template<typename T = void>
	[[nodiscard]] T const* data(ULONG const offset) const
	{
		auto const base = reinterpret_cast<std::byte const*>(m_mount_points);
		return reinterpret_cast<T const*>(base + offset);
	}
};

class mount_point_view : mount_point_view_base
{
	MOUNTMGR_MOUNT_POINT const* m_mount_point;

public:
	explicit mount_point_view(
		MOUNTMGR_MOUNT_POINTS const* const mount_points,
		MOUNTMGR_MOUNT_POINT const* const mount_point)
		: mount_point_view_base(mount_points)
		, m_mount_point(mount_point)
	{
	}

	[[nodiscard]] std::wstring_view symbolic_link_name() const
	{
		return std::wstring_view(
			data<wchar_t>(m_mount_point->SymbolicLinkNameOffset),
			m_mount_point->SymbolicLinkNameLength / sizeof(wchar_t));
	}

	[[nodiscard]] std::wstring_view device_name() const
	{
		return std::wstring_view(
			data<wchar_t>(m_mount_point->DeviceNameOffset),
			m_mount_point->DeviceNameLength / sizeof(wchar_t));
	}

	[[nodiscard]] std::span<std::byte const> unique_id() const
	{
		return std::span<std::byte const>(
			data<std::byte>(m_mount_point->UniqueIdOffset),
			m_mount_point->UniqueIdLength);
	}
};

class mount_points_view : mount_point_view_base
{
public:
	class iterator
	{
		MOUNTMGR_MOUNT_POINTS const* m_mount_points;
		MOUNTMGR_MOUNT_POINT const* m_mount_point;

	public:
		using value_type = mount_point_view;
		using difference_type = ptrdiff_t;

		explicit iterator(
			MOUNTMGR_MOUNT_POINTS const* const mount_points,
			MOUNTMGR_MOUNT_POINT const* const mount_point)
			: m_mount_points(mount_points)
			, m_mount_point(mount_point)
		{
		}

		[[nodiscard]] mount_point_view operator*() const
		{
			return mount_point_view(m_mount_points, m_mount_point);
		}

		[[nodiscard]] vsm::arrow<mount_point_view> operator->() const
		{
			return vsm::arrow<mount_point_view>(std::in_place, m_mount_points, m_mount_point);
		}

		iterator& operator++() &
		{
			++m_mount_point;
			return *this;
		}

		[[nodiscard]] iterator operator++(int) &
		{
			auto r = *this;
			++m_mount_point;
			return r;
		}

		[[nodiscard]] friend bool operator==(iterator const& lhs, iterator const& rhs)
		{
			return lhs.m_mount_point == rhs.m_mount_point;
		}
	};

	explicit mount_points_view(MOUNTMGR_MOUNT_POINTS const* const mount_points)
		: mount_point_view_base(mount_points)
	{
	}

	[[nodiscard]] size_t size() const
	{
		return m_mount_points->NumberOfMountPoints;
	}

	[[nodiscard]] mount_point_view operator[](size_t const index) const
	{
		vsm_assert(index < m_mount_points->NumberOfMountPoints);
		return mount_point_view(m_mount_points, m_mount_points->MountPoints + index);
	}

	[[nodiscard]] iterator begin() const
	{
		return iterator(m_mount_points, m_mount_points->MountPoints);
	}

	[[nodiscard]] iterator end() const
	{
		return iterator(
			m_mount_points,
			m_mount_points->MountPoints + m_mount_points->NumberOfMountPoints);
	}
};


using mount_points_storage_type =
	untyped_storage<sizeof(MOUNTMGR_MOUNT_POINTS) + 2 * MAX_PATH * sizeof(wchar_t)>;

static vsm::result<mount_points_view> query_mount_points(
	untyped_storage_base& storage,
	std::wstring_view const device_name)
{
	vsm_try(mount_manager, open_mount_manager());

	untyped_storage<sizeof(MOUNTMGR_MOUNT_POINT) + 64 * sizeof(wchar_t)> name_storage;
	vsm_try_bind((name, name_size), get_mount_point_name(name_storage, device_name));

	while (true)
	{
		auto const mount_points = reinterpret_cast<MOUNTMGR_MOUNT_POINTS*>(storage.data());

		IO_STATUS_BLOCK io_status_block;
		NTSTATUS const status = win32::NtDeviceIoControlFile(
			mount_manager,
			/* Event: */ NULL,
			/* ApcRoutine: */ nullptr,
			/* ApcContext: */ nullptr,
			&io_status_block,
			IOCTL_MOUNTMGR_QUERY_POINTS,
			name,
			vsm::truncating(name_size),
			mount_points,
			vsm::truncating(storage.size()));

		if (NT_SUCCESS(status))
		{
			return mount_points_view(mount_points);
		}

		if (status != STATUS_BUFFER_OVERFLOW)
		{
			return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
		}

		vsm_try_discard(storage.resize(
			sizeof(MOUNTMGR_MOUNT_POINTS) + mount_points->Size,
			resize_mode::discard));
	}
}


static std::optional<std::wstring_view> parse_dos_mount_name(std::wstring_view mount_name)
{
	constexpr std::wstring_view dos_prefix = L"\\DosDevices\\";

	if (mount_name.starts_with(dos_prefix))
	{
		mount_name.remove_prefix(dos_prefix.size());

		if (mount_name.size() == 2 && mount_name.back() == L':')
		{
			wchar_t const drive_letter = mount_name.front();
			if (L'A' <= drive_letter && drive_letter <= 'Z')
			{
				return mount_name;
			}
		}
	}

	return std::nullopt;
}

static std::optional<std::wstring_view> parse_uid_mount_name(std::wstring_view const mount_name)
{
	constexpr std::wstring_view uid_prefix = L"\\??\\Volume{";
	constexpr std::wstring_view uid_suffix = L"}";

	std::wstring_view name = mount_name;

	if (!name.starts_with(uid_prefix))
	{
		return std::nullopt;
	}
	name.remove_prefix(uid_prefix.size());

	if (!name.ends_with(uid_suffix))
	{
		return std::nullopt;
	}
	name.remove_suffix(uid_suffix.size());

	if (name.size() != 36)
	{
		return std::nullopt;
	}

	for (auto const [i, x] : std::views::enumerate(name))
	{
		if (i == 8 || i == 13 || i == 18 || i == 23)
		{
			if (x != L'-')
			{
				return std::nullopt;
			}
		}
		else
		{
			if (!(L'0' <= x && x <= L'9' || L'a' <= x && x <= L'f'))
			{
				return std::nullopt;
			}
		}
	}

	return mount_name;
}

static vsm::result<std::wstring_view> query_file_name_ntapi(
	basic_storage_base<wchar_t>& storage,
	HANDLE const handle,
	path_formats const formats)
{
	name_information_storage_type object_path_storage;
	vsm_try(object_path, query_object_path(object_path_storage, handle));
	vsm_try(device_name, get_device_name(object_path));

	uint8_t dos_priority = static_cast<uint8_t>(-1);
	uint8_t uid_priority = static_cast<uint8_t>(-1);
	uint8_t dev_priority = static_cast<uint8_t>(-1);

	for (auto const [index, format] : std::views::enumerate(formats))
	{
		switch (std::to_underlying(format))
		{
		case std::to_underlying(dos_format):
			if (dos_priority == static_cast<uint8_t>(-1))
			{
				dos_priority = vsm::truncating(index + 1);
			}
			break;

		case std::to_underlying(uid_format):
			if (uid_priority == static_cast<uint8_t>(-1))
			{
				uid_priority = vsm::truncating(index + 1);
			}
			break;

		case std::to_underlying(dev_format):
			if (dev_priority == static_cast<uint8_t>(-1))
			{
				dev_priority = vsm::truncating(index + 1);
			}
			break;
		}
	}

	std::wstring_view path_prefix = L"\\??\\GLOBALROOT";
	std::wstring_view volume_path = device_name;
	uint8_t best_volume_path = dev_priority;

	mount_points_storage_type mount_points_storage;
	if (dos_priority < dev_priority || uid_priority < dev_priority)
	{
		vsm_try(mount_points, query_mount_points(mount_points_storage, device_name));

		for (mount_point_view const mount_point : mount_points)
		{
			auto const mount_name = mount_point.symbolic_link_name();

			if (dos_priority < best_volume_path)
			{
				if (auto const dos_name = parse_dos_mount_name(mount_name))
				{
					volume_path = *dos_name;
					path_prefix = std::wstring_view();
					best_volume_path = dos_priority;
					continue;
				}
			}

			if (uid_priority < best_volume_path)
			{
				if (auto const uid_name = parse_uid_mount_name(mount_name))
				{
					volume_path = *uid_name;
					path_prefix = std::wstring_view();
					best_volume_path = uid_priority;
					continue;
				}
			}
		}
	}

	name_information_storage_type normalized_path_storage;
	vsm_try(normal_path, query_normalized_file_path(normalized_path_storage, handle));

	vsm_try(path_storage, storage.resize(
		path_prefix.size() + volume_path.size() + normal_path.size(),
		resize_mode::discard));

	wchar_t* path_end = path_storage.data();

	path_end = std::copy_n(path_prefix.data(), path_prefix.size(), path_end);
	path_end = std::copy_n(volume_path.data(), volume_path.size(), path_end);
	path_end = std::copy_n(normal_path.data(), normal_path.size(), path_end);

	return std::wstring_view(path_storage.data(), path_end);
}

static vsm::result<std::wstring_view> query_file_name_win32(
	basic_storage_base<wchar_t>& storage,
	HANDLE const handle,
	path_formats const formats)
{
	for (path_format const format : formats)
	{
		DWORD flags = FILE_NAME_NORMALIZED;

		switch (std::to_underlying(format))
		{
		case std::to_underlying(dos_format):
			flags |= VOLUME_NAME_DOS;
			break;

		case std::to_underlying(uid_format):
			flags |= VOLUME_NAME_GUID;
			break;

		case std::to_underlying(dev_format):
			flags |= VOLUME_NAME_NT;
			break;

		default:
			vsm_unreachable();
		}

		std::wstring_view const root = format != dev_format
			? std::wstring_view()
			: std::wstring_view(L"\\??\\GLOBALROOT");

		while (true)
		{
			vsm_assert(storage.size() >= root.size());

			wchar_t* const buffer = reinterpret_cast<wchar_t*>(storage.data());
			size_t const buffer_writable_size = storage.size() - root.size();

			DWORD const name_size = GetFinalPathNameByHandleW(
				handle,
				buffer + root.size(),
				vsm::truncating(buffer_writable_size),
				flags);

			if (name_size == 0)
			{
				DWORD const error = GetLastError();

				if (error == ERROR_PATH_NOT_FOUND)
				{
					break;
				}

				return vsm::unexpected(allio_error(static_cast<system_error>(error)));
			}

			if (name_size < buffer_writable_size)
			{
				std::copy(root.begin(), root.end(), buffer);

				auto path = std::wstring_view(
					buffer,
					root.size() + name_size);

				if (path.starts_with(L"\\\\?\\"))
				{
					if (format == dos_format)
					{
						path.remove_prefix(4);
					}
					else
					{
						const_cast<wchar_t*>(path.data())[1] = L'?';
					}
				}

				return path;
			}

			vsm_try_discard(storage.resize(root.size() + name_size, resize_mode::discard));
		}
	}

	return vsm::unexpected(allio_error(static_cast<system_error>(ERROR_PATH_NOT_FOUND)));
}

} // namespace

vsm::result<std::wstring_view> win32::query_file_name(
	basic_storage_base<wchar_t>& storage,
	HANDLE const handle,
	path_formats formats)
{
	if (formats.empty())
	{
		formats = dos_format | uid_format | dev_format;
	}

#if allio_config_ntapi >= allio_ntapi_always || 1
	(void)&query_file_name_win32;
	return query_file_name_ntapi(storage, handle, formats);
#else
	(void)&query_file_name_ntapi;
	return query_file_name_win32(storage, handle, formats);
#endif
}


static vsm::result<unique_handle> try_open_anchor(
	HANDLE const base,
	std::wstring_view const path,
	bool const may_open_leaf)
{
	vsm::result<unique_handle> result(vsm::result_value);

	if (auto r = open_anchor_file(base, path, may_open_leaf))
	{
		result = vsm_move(r);
	}
	else if (r.error().default_error_condition() != std::errc::no_such_file_or_directory)
	{
		r = vsm::unexpected(r.error());
	}

	return result;
}

using handle_path_pair = std::pair<HANDLE, std::wstring_view>;

static vsm::result<handle_path_pair> open_longest_path(
	kernel_path_storage_base& path_storage,
	unique_handle& out_path_handle,
	fs_path const& path,
	bool const requires_fully_valid_path)
{
	HANDLE const base_handle = path.base == nullptr
		? NULL
		: unwrap_handle(path.base->platform_handle);

	kernel_path_handle path_handle;
	vsm_try(kernel_path, make_kernel_path(path_storage, base_handle, path.path, &path_handle));
	vsm_try(h, try_open_anchor(path_handle.get(), kernel_path, /* may_open_leaf: */ true));

	if (h)
	{
		HANDLE const handle = h.get();
		out_path_handle = vsm_move(h);

		return handle_path_pair(handle, std::wstring_view());
	}

	if (requires_fully_valid_path)
	{
		return vsm::unexpected(
			allio_error(static_cast<kernel_error>(STATUS_OBJECT_PATH_NOT_FOUND)));
	}

	if (path_handle.get() == NULL)
	{
		vsm_assert(kernel_path.starts_with(L"\\??\\"));

		size_t slash = kernel_path.size();
		while (slash >= 4)
		{
			slash = kernel_path.find(L'\\', slash);
			vsm_assert(slash != kernel_path.npos);

			// Try opening the next parent directory:
			vsm_try(new_h, try_open_anchor(
				/* base: */ NULL,
				kernel_path.substr(0, slash),
				/* may_open_leaf: */ false));

			if (new_h)
			{
				HANDLE const handle = new_h.get();
				out_path_handle = vsm_move(new_h);

				return handle_path_pair(handle, kernel_path.substr(0, slash));
			}
		}

		return handle_path_pair(NULL, kernel_path);
	}
	else
	{
		vsm_assert(!kernel_path.starts_with(L'\\'));

		HANDLE handle = path_handle.get();
		std::wstring_view relative_path = kernel_path;

		while (!relative_path.empty())
		{
			size_t const slash = relative_path.find(L'\\');

			if (slash == std::wstring_view::npos)
			{
				break;
			}

			// Try opening the next child directory:
			vsm_try(new_h, try_open_anchor(
				handle,
				relative_path.substr(0, slash),
				/* may_open_leaf: */ false));

			if (!new_h)
			{
				break;
			}

			// Unlock the PEB (if locked) as soon as possible.
			path_handle.close();

			h = vsm_move(new_h);
			handle = h.get();

			relative_path.remove_prefix(slash + 1);
		}

		if (handle == path_handle.get())
		{
			// If the selected handle is owned by the PEB, it must be duplicated before returning.
			vsm_try_assign(out_path_handle, path_handle.duplicate());
		}
		else
		{
			// h may be null in the case that the returned handle is the original base handle.
			out_path_handle = vsm_move(h);
		}

		return handle_path_pair(handle, relative_path);
	}
}

vsm::result<size_t> detail::canonical_path(fs_path const& path, any_path_buffer const buffer)
{
	basic_storage<wchar_t, MAX_PATH + 4> path_storage;
	unique_handle path_handle;

	vsm_try_bind((h, invalid_path), open_longest_path(
		path_storage,
		path_handle,
		path,
		/* requires_fully_valid_path: */ true));

	vsm_assert(h && invalid_path.empty());

	vsm_try(full_path, query_file_name(path_storage, h, path_formats()));

	if (full_path.starts_with(L"\\??\\"))
	{
		const_cast<wchar_t*>(full_path.data())[1] = L'\\';
	}

	return copy_or_transcode_string(full_path, buffer.string());
}

vsm::result<size_t> _weakly_canonical_path(fs_path const& path, any_string_buffer const buffer)
{
	basic_storage<wchar_t, MAX_PATH + 4> path_storage_1;
	unique_handle path_handle;

	vsm_try_bind((h, invalid_path), open_longest_path(
		path_storage_1,
		path_handle,
		path,
		/* requires_fully_valid_path: */ false));

	basic_storage<wchar_t, MAX_PATH + 4> path_storage_2;
	std::wstring_view full_path = invalid_path;

	if (h)
	{
		vsm_try(valid_path, query_file_name(path_storage_2, h, path_formats()));

		if (invalid_path.empty())
		{
			full_path = valid_path;
		}
		else
		{
			vsm_try(data, path_storage_2.resize(valid_path.size() + 1 + invalid_path.size()));
			wchar_t* data_end = data.data() + valid_path.size();

			*data_end++ = L'\\';
			data_end = std::copy(invalid_path.begin(), invalid_path.end(), data_end);

			full_path = std::wstring_view(data.data(), data_end);
		}
	}

	if (full_path.starts_with(L"\\??\\"))
	{
		const_cast<wchar_t*>(full_path.data())[1] = L'\\';
	}

	if (buffer.char_type() == char_type::_wchar_t)
	{
		// Normalize directly into the user buffer:
		vsm_try(normal_path, win32::path_traits<wchar_t>::copy_lexically_normal(
			full_path,
			buffer.buffer<wchar_t>()));

		if (buffer.encoding() == encoding_family::utf)
		{
			vsm_try_void(validate_utf(normal_path));
		}

		return normal_path.size();
	}
	else
	{
		// Normalize first into a temporary buffer and then transcode:
		vsm_try(normal_path, win32::path_traits<wchar_t>::copy_lexically_normal(
			full_path,
			string_buffer<wchar_t>(path_storage_1)));

		return transcode_string(normal_path, buffer);
	}
}

vsm::result<size_t> detail::weakly_canonical_path(fs_path const& path, any_path_buffer const buffer)
{
	return _weakly_canonical_path(path, buffer.string());
}

#if 0
static vsm::result<size_t> _relative_path(
	fs_path const& path,
	fs_path const& base,
	any_string_buffer const buffer)
{
	basic_storage<wchar_t, MAX_PATH + 4> path_storage;
	vsm_try(path_size, _weakly_canonical_path(path, path_storage));

	basic_storage<wchar_t, MAX_PATH + 4> base_storage;
	vsm_try(base_size, _weakly_canonical_path(base, base_storage));

	auto const canonical_path = std::wstring_view(path_storage.data(), path_size);
	auto const canonical_base = std::wstring_view(base_storage.data(), base_size);

	if (buffer.char_type() == char_type::_wchar_t)
	{
		vsm_try(relative_path, win32::path_traits<wchar_t>::copy_lexically_relative(
			canonical_path,
			canonical_base,
			buffer.buffer<wchar_t>()));

		if (buffer.encoding() == encoding_family::utf)
		{
			vsm_try_void(validate_utf(relative_path));
		}

		return relative_path.size();
	}
	else
	{
		basic_storage<wchar_t, MAX_PATH + 4> relative_path_storage;
		vsm_try(relative_path, win32::path_traits<wchar_t>::copy_lexically_relative(
			canonical_path,
			canonical_base,
			string_buffer<wchar_t>(relative_path_storage)));

		return transcode_string(relative_path, buffer);
	}
}

vsm::result<size_t> detail::relative_path(
	fs_path const& path,
	fs_path const& base,
	any_path_buffer const buffer)
{
	return _relative_path(path, base, buffer.string());
}

static vsm::result<size_t> _proximate_path(
	fs_path const& path,
	fs_path const& base,
	any_string_buffer const buffer)
{
	basic_storage<wchar_t, MAX_PATH + 4> path_storage;
	vsm_try(path_size, _weakly_canonical_path(path, path_storage));

	basic_storage<wchar_t, MAX_PATH + 4> base_storage;
	vsm_try(base_size, _weakly_canonical_path(base, base_storage));

	auto const canonical_path = std::wstring_view(path_storage.data(), path_size);
	auto const canonical_base = std::wstring_view(base_storage.data(), base_size);

	if (buffer.char_type() == char_type::_wchar_t)
	{
		vsm_try(proximate_path, win32::path_traits<wchar_t>::copy_lexically_proximate(
			canonical_path,
			canonical_base,
			buffer.buffer<wchar_t>()));

		if (buffer.encoding() == encoding_family::utf)
		{
			vsm_try_void(validate_utf(proximate_path));
		}

		return proximate_path.size();
	}
	else
	{
		basic_storage<wchar_t, MAX_PATH + 4> proximate_path_storage;
		vsm_try(proximate_path, win32::path_traits<wchar_t>::copy_lexically_proximate(
			canonical_path,
			canonical_base,
			string_buffer<wchar_t>(proximate_path_storage)));

		return transcode_string(proximate_path, buffer);
	}
}

vsm::result<size_t> detail::proximate_path(
	fs_path const& path,
	fs_path const& base,
	any_path_buffer const buffer)
{
	return _proximate_path(path, base, buffer.string());
}
#endif
