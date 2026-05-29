#include <allio/impl/linux/filesystem.hpp>

#include <allio/impl/lazy_feature_flag.hpp>
#include <allio/impl/linux/api_string.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/syscall.hpp>

#include <vsm/atomic.hpp>
#include <vsm/numeric.hpp>

#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

template<typename Timespec>
static fs_time_point get_file_time(Timespec const& time)
{
	return fs_time_point(fs_duration(time.tv_sec * 1'000'000'000 + time.tv_nsec));
}

fs_time_point fs_clock::now() noexcept
{
	timespec time;

	if (clock_gettime(CLOCK_REALTIME, &time) == -1)
	{
		return fs_time_point();
	}

	return get_file_time(time);
}


vsm::result<unique_handle> linux::create_file(
	int dir_fd,
	char const *const path,
	int const flags,
	mode_t const mode)
{
	if (dir_fd == -1)
	{
		dir_fd = AT_FDCWD;
	}

	int const fd = allio_syscall(openat)(
		dir_fd,
		path,
		flags,
		mode);

	if (fd == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	return vsm::result<unique_handle>(vsm::result_value, fd);
}

vsm::result<unique_handle> linux::create_file(
	int const dir_fd,
	char const *const path,
	platform_open_options const &options)
{
	return create_file(dir_fd, path, options.flags, options.mode);
}

vsm::result<unique_handle> linux::reopen_file(
	int const fd,
	int const flags,
	mode_t const mode)
{
	if (flags & O_DIRECTORY)
	{
		return linux::create_file(
			fd,
			".",
			flags,
			mode);
	}

	char link_path[32];
	vsm_verify(snprintf(link_path, sizeof(link_path), "/proc/self/fd/%d", fd) > 0);

	int const new_fd = allio_syscall(open)(
		link_path,
		flags,
		mode);

	if (new_fd == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	return vsm::result<unique_handle>(vsm::result_value, new_fd);
}

vsm::result<unique_handle> linux::reopen_file(
	int const fd,
	platform_open_options const &options)
{
	return reopen_file(fd, options.flags, options.mode);
}


vsm::result<unique_handle> linux::open_anchor_file(
	int const dir_fd,
	char const *const path,
	bool const may_open_leaf)
{
	platform_open_options options =
	{
		.flags = O_CLOEXEC,
	};

	if (!may_open_leaf)
	{
		options.flags |= O_PATH | O_DIRECTORY;
	}

	return create_file(dir_fd, path, options);
}

vsm::result<unique_handle> detail::open_anchor_file(
	int const dir_fd,
	any_path_view const path,
	bool const may_open_leaf)
{
	api_string_storage storage;
	vsm_try(path_c_str, make_api_c_string(storage, path.string()));
	return linux::open_anchor_file(dir_fd, path_c_str, may_open_leaf);
}

vsm::result<unique_handle> detail::open_file(
	int const dir_fd,
	any_path_view const path,
	platform_open_options const &options)
{
	api_string_storage storage;
	vsm_try(path_c_str, make_api_c_string(storage, path.string()));
	return linux::create_file(dir_fd, path_c_str, options);
}

vsm::result<unique_handle> detail::open_anonymous_file(
	int const dir_fd,
	platform_open_options const &options)
{
	// Anonymous files can only be opened as create_only.
	vsm_assert(vsm::all_flags(options.flags, O_CREAT | O_EXCL));

	platform_open_options anonymous_options = options;
	anonymous_options.flags &= ~(O_CREAT | O_EXCL);
	anonymous_options.flags |= O_TMPFILE;

	return linux::create_file(dir_fd, ".", anonymous_options);
}


vsm::result<void> linux::link_file_at(
	int const fd,
	int dir_fd,
	char const *const path,
	bool const replace_existing_file)
{
	if (replace_existing_file)
	{
		return vsm::unexpected(error::unsupported_operation);
	}

	if (dir_fd == -1)
	{
		dir_fd = AT_FDCWD;
	}

	char link_path[32];
	vsm_verify(snprintf(link_path, sizeof(link_path), "/proc/self/fd/%d", fd) > 0);

	int const r = linkat(-1, link_path, dir_fd, path, AT_SYMLINK_FOLLOW);

	if (r == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	return {};
}

vsm::result<void> detail::link_file_at(
	int const fd,
	int const dir_fd,
	any_path_view const path,
	bool const replace_existing_file)
{
	if (replace_existing_file)
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	api_string_storage storage;
	vsm_try(path_c_str, make_api_c_string(storage, path.string()));
	return linux::link_file_at(fd, dir_fd, path_c_str, replace_existing_file);
}


static fs_type get_file_type(mode_t const mode)
{
	if (S_ISREG(mode))
	{
		return fs_type::regular;
	}

	if (S_ISDIR(mode))
	{
		return fs_type::directory;
	}

	if (S_ISCHR(mode))
	{
		return fs_type::character_device;
	}

	if (S_ISBLK(mode))
	{
		return fs_type::block_device;
	}

	if (S_ISFIFO(mode))
	{
		return fs_type::pipe;
	}

	if (S_ISLNK(mode))
	{
		return fs_type::symbolic_link;
	}

	if (S_ISSOCK(mode))
	{
		return fs_type::socket;
	}

	return fs_type::unknown;
}


fs_type detail::_fs_info_type(void const* const info)
{
	auto const stat = static_cast<struct statx const*>(info);
	vsm_assert(vsm::all_flags(stat->stx_mask, STATX_TYPE));
	return get_file_type(stat->stx_mode);
}

fs_dev_id detail::_fs_info_dev_id(void const* const info)
{
	auto const stat = static_cast<struct statx const*>(info);
	return static_cast<fs_dev_id>(makedev(stat->stx_dev_major, stat->stx_dev_minor));
}

fs_ino_id detail::_fs_info_ino_id(void const* const info)
{
	auto const stat = static_cast<struct statx const*>(info);
	vsm_assert(vsm::all_flags(stat->stx_mask, STATX_INO));
	return static_cast<fs_ino_id>(stat->stx_ino);
}

fs_size detail::_fs_info_link_count(void const* const info)
{
	auto const stat = static_cast<struct statx const*>(info);
	vsm_assert(vsm::all_flags(stat->stx_mask, STATX_NLINK));
	return stat->stx_nlink;
}

fs_size detail::_fs_info_max_extent(void const* const info)
{
	auto const stat = static_cast<struct statx const*>(info);
	vsm_assert(vsm::all_flags(stat->stx_mask, STATX_SIZE));
	return stat->stx_size;
}

fs_time_point detail::_fs_info_create_time(void const* const info)
{
	auto const stat = static_cast<struct statx const*>(info);
	vsm_assert(vsm::all_flags(stat->stx_mask, STATX_BTIME));
	return get_file_time(stat->stx_btime);
}

fs_time_point detail::_fs_info_data_access_time(void const* const info)
{
	auto const stat = static_cast<struct statx const*>(info);
	vsm_assert(vsm::all_flags(stat->stx_mask, STATX_ATIME));
	return get_file_time(stat->stx_atime);
}

fs_time_point detail::_fs_info_data_change_time(void const* const info)
{
	auto const stat = static_cast<struct statx const*>(info);
	vsm_assert(vsm::all_flags(stat->stx_mask, STATX_MTIME));
	return get_file_time(stat->stx_mtime);
}

fs_time_point detail::_fs_info_attr_change_time(void const* const info)
{
	auto const stat = static_cast<struct statx const*>(info);
	vsm_assert(vsm::all_flags(stat->stx_mask, STATX_CTIME));
	return get_file_time(stat->stx_ctime);
}

fs_io_limits detail::_fs_info_io_limits(void const* const info)
{
	auto const stat = static_cast<struct statx const*>(info);
	vsm_assert(vsm::all_flags(stat->stx_mask, STATX_DIOALIGN));

	fs_io_limits limits =
	{
		.minimum_buffer_alignment = stat->stx_dio_mem_align,

		.minimum_reading_alignment = stat->stx_dio_offset_align,
		.optimal_reading_alignment = stat->stx_dio_offset_align,

		.minimum_writing_alignment = stat->stx_dio_offset_align,
		.optimal_writing_alignment = stat->stx_dio_offset_align,
	};

	if (vsm::all_flags(stat->stx_mask, STATX_WRITE_ATOMIC))
	{
		limits.minimum_atomic_write_size = stat->stx_atomic_write_unit_min;
		limits.optimal_atomic_write_size = stat->stx_atomic_write_unit_max;
		limits.maximum_atomic_write_size = stat->stx_atomic_write_unit_max;

		if (stat->stx_atomic_write_unit_max_opt != 0)
		{
			limits.optimal_atomic_write_size = stat->stx_atomic_write_unit_max_opt;
		}
	}

	if (vsm::all_flags(stat->stx_mask, STATX_DIO_READ_ALIGN))
	{
		if (stat->stx_dio_read_offset_align != 0)
		{
			if (stat->stx_dio_read_offset_align < limits.minimum_reading_alignment)
			{
				limits.minimum_reading_alignment = stat->stx_dio_read_offset_align;
			}

			limits.optimal_reading_alignment = stat->stx_dio_read_offset_align;
		}
	}

	return limits;
}


static statx_timestamp make_statx_timestamp(timespec const& time)
{
	return statx_timestamp
	{
		.tv_sec = time.tv_sec,
		.tv_nsec = vsm::truncating(time.tv_nsec),
	};
}

static unsigned make_statx_mask_from_info(fs_info_mask const mask)
{
	unsigned statx_mask = 0;

	if (vsm::any_flags(mask, fs_info_mask::type))
	{
		statx_mask |= STATX_TYPE;
	}

	if (vsm::any_flags(mask, fs_info_mask::ino_id))
	{
		statx_mask |= STATX_INO;
	}

	if (vsm::any_flags(mask, fs_info_mask::link_count))
	{
		statx_mask |= STATX_NLINK;
	}

	if (vsm::any_flags(mask, fs_info_mask::max_extent))
	{
		statx_mask |= STATX_SIZE;
	}

	if (vsm::any_flags(mask, fs_info_mask::create_time))
	{
		statx_mask |= STATX_BTIME;
	}

	if (vsm::any_flags(mask, fs_info_mask::data_access_time))
	{
		statx_mask |= STATX_ATIME;
	}

	if (vsm::any_flags(mask, fs_info_mask::data_change_time))
	{
		statx_mask |= STATX_MTIME;
	}

	if (vsm::any_flags(mask, fs_info_mask::attr_change_time))
	{
		statx_mask |= STATX_CTIME;
	}

	if (vsm::any_flags(mask, fs_info_mask::io_limits))
	{
		statx_mask |= STATX_DIOALIGN | STATX_WRITE_ATOMIC | STATX_DIO_READ_ALIGN;
	}

	return statx_mask;
}

static fs_info_mask make_info_mask_from_statx(uint32_t const statx_mask)
{
	fs_info_mask mask = fs_info_mask::dev_id;

	if (vsm::all_flags(statx_mask, STATX_TYPE))
	{
		mask |= fs_info_mask::type;
	}

	if (vsm::all_flags(statx_mask, STATX_NLINK))
	{
		mask |= fs_info_mask::link_count;
	}

	if (vsm::all_flags(statx_mask, STATX_ATIME))
	{
		mask |= fs_info_mask::data_access_time;
	}

	if (vsm::all_flags(statx_mask, STATX_MTIME))
	{
		mask |= fs_info_mask::data_change_time;
	}

	if (vsm::all_flags(statx_mask, STATX_CTIME))
	{
		mask |= fs_info_mask::attr_change_time;
	}

	if (vsm::all_flags(statx_mask, STATX_INO))
	{
		mask |= fs_info_mask::ino_id;
	}

	if (vsm::all_flags(statx_mask, STATX_SIZE))
	{
		mask |= fs_info_mask::max_extent;
	}

	if (vsm::all_flags(statx_mask, STATX_BTIME))
	{
		mask |= fs_info_mask::create_time;
	}

	if (vsm::all_flags(statx_mask, STATX_DIOALIGN))
	{
		mask |= fs_info_mask::io_limits;
	}

	return mask;
}


static constexpr size_t statx_stat_end = offsetof(struct statx, stx_mnt_id);

static bool is_statx_supported()
{
	static constexpr auto query_statx_support = []() -> bool
	{
		vsm_gnu_diagnostic(push)
		vsm_gnu_diagnostic(ignored "-Wnonnull")

		errno = 0;
		::statx(-1, nullptr, 0, 0, nullptr);
		return errno != ENOSYS;

		vsm_gnu_diagnostic(pop)
	};

	static constinit lazy_feature_flag is_supported;
	return is_supported.query(query_statx_support);
}

static vsm::result<fs_info_view> _query_file_info_statx(
	int const dir_fd,
	char const* const path,
	int const flags,
	fs_info_mask const mask,
	std::span<std::byte> const storage)
{
	vsm_assert(storage.size() >= sizeof(struct statx));

	if (!is_statx_supported())
	{
		return fs_info_view();
	}

	auto const stat = reinterpret_cast<struct statx*>(storage.data());

	if (::statx(dir_fd, path, flags, make_statx_mask_from_info(mask), stat) == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	auto const result_mask = make_info_mask_from_statx(stat->stx_mask);

	if (vsm::no_flags(result_mask, mask))
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	return fs_info_view(stat, result_mask);
}

static vsm::result<fs_info_view> _query_file_info_stat(
	int const dir_fd,
	char const* const path,
	int const flags,
	fs_info_mask const mask,
	std::span<std::byte> const storage)
{
	vsm_assert(storage.size() >= statx_stat_end);

	struct stat local_stat;
	if (::fstatat(dir_fd, path, &local_stat, flags) == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	// TODO: This is technically UB if the storage is not large enough.
	//       Consider using a memcpy based solution to access data instead.
	auto const statx = vsm::start_lifetime_as<struct statx>(storage.data());

	statx->stx_mask = STATX_BASIC_STATS;
	statx->stx_nlink = vsm::truncating(local_stat.st_nlink);
	statx->stx_uid = local_stat.st_uid;
	statx->stx_gid = local_stat.st_gid;
	statx->stx_mode = vsm::truncating(local_stat.st_mode);
	statx->stx_ino = local_stat.st_ino;
	statx->stx_size = vsm::truncating(local_stat.st_size);
	statx->stx_blocks = vsm::truncating(local_stat.st_blocks);
	statx->stx_atime = make_statx_timestamp(local_stat.st_atim);
	statx->stx_ctime = make_statx_timestamp(local_stat.st_ctim);
	statx->stx_mtime = make_statx_timestamp(local_stat.st_mtim);
	statx->stx_dev_major = major(local_stat.st_dev);
	statx->stx_dev_minor = minor(local_stat.st_dev);

	return fs_info_view(statx, fs_info_mask::standard);
}

static vsm::result<fs_info_view> _query_file_info(
	int const dir_fd,
	char const* const path,
	int const flags,
	fs_info_mask const mask,
	any_byte_buffer_ref const buffer)
{
	std::span<std::byte> storage;

	if (auto const r = buffer.resize(sizeof(struct statx)))
	{
		vsm_try(view, _query_file_info_statx(dir_fd, path, flags, mask, *r));

		if (view)
		{
			return view;
		}
		else if (vsm::no_flags(mask, fs_info_mask::standard))
		{
			return vsm::unexpected(allio_error(error::unsupported_operation));
		}
		else
		{
			storage = *r;
		}
	}
	else if (vsm::no_flags(mask, fs_info_mask::standard))
	{
		return vsm::unexpected(r.error());
	}
	else
	{
		vsm_try_assign(storage, buffer.resize(statx_stat_end));
	}

	return _query_file_info_stat(dir_fd, path, flags, mask, storage);
}

vsm::result<fs_info_view> linux::query_path_info(
	int dir_fd,
	char const* const path,
	fs_info_mask const mask,
	any_byte_buffer_ref const buffer)
{
	if (dir_fd == -1)
	{
		dir_fd = AT_FDCWD;
	}

	return _query_file_info(dir_fd, path, /* flags: */ 0, mask, buffer);
}

vsm::result<fs_info_view> detail::query_file_info(
	int const fd,
	fs_info_mask const mask,
	any_byte_buffer_ref const buffer)
{
	return _query_file_info(fd, "", AT_EMPTY_PATH, mask, buffer);
}

vsm::result<fs_info_view> detail::query_path_info(
	int const fd,
	any_path_view const path,
	fs_info_mask const mask,
	any_byte_buffer_ref const buffer)
{
	api_string_storage storage;
	vsm_try(path_c_str, make_api_c_string(storage, path.string()));
	return linux::query_path_info(fd, path_c_str, mask, buffer);
}
