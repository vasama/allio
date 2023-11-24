#pragma once

#include <allio/any_path.hpp>
#include <allio/platform.hpp>

#include <vsm/flags.hpp>
#include <vsm/result.hpp>

#include <chrono>
#include <compare>
#include <concepts>

#include <cstddef>
#include <cstdint>

namespace allio::detail {

using fs_size = uint64_t;

using fs_clock = std::chrono::file_clock;
using fs_time_point = fs_clock::time_point;


class fs_device_id
{
	uint64_t m_id;

	friend auto operator<=>(fs_device_id const&, fs_device_id const&) = default;
};

class fs_node_id
{
	uint64_t m_id;

	friend auto operator<=>(fs_node_id const&, fs_node_id const&) = default;
};

class fs_permissions
{
};


enum class fs_entry_type : unsigned char
{
	unknown,

	regular,
	directory,

	pipe,
	socket,

	block_device,
	character_device,

	symbolic_link,

	ntfs_junction,
};

struct fs_entry_info
{
	fs_entry_type type;
	fs_permissions permissions;

	fs_device_id device_id;
	fs_node_id node_id;

	fs_size file_end_offset;
	fs_size hard_link_count;

	fs_time_point last_data_access_time;
	fs_time_point last_data_modify_time;
	fs_time_point last_node_modify_time;
};


struct fs_path
{
	native_platform_handle base;
	any_path_view path;

	template<std::convertible_to<any_path_view> Path>
	fs_path(Path const& path)
		: base(native_platform_handle::null)
		, path(path)
	{
	}
	
	template<std::convertible_to<any_path_view> Path>
	explicit fs_path(native_platform_handle const base, Path const& path)
		: base(base)
		, path(path)
	{
	}
};

struct fs_path_t
{
	fs_path path;
};


vsm::result<fs_entry_info> get_fs_entry_info(fs_path path);

} // namespace allio::detail
