#pragma once

#include <allio/any_path.hpp>
#include <allio/detail/object_concepts.hpp>

#include <vsm/flags.hpp>
#include <vsm/result.hpp>

#include <chrono>
#include <compare>
#include <concepts>

#include <cstddef>
#include <cstdint>

namespace allio::detail {

struct fs_object_t;


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
	native_handle<fs_object_t> const* base = nullptr;
	any_path_view path;

	fs_path() = default;

	template<detail::handle_for<detail::fs_object_t> Handle>
	fs_path(Handle const& base)
		: base(&base.native())
		, path{}
	{
	}

	template<std::convertible_to<any_path_view> Path>
	fs_path(Path const& path)
		: base(nullptr)
		, path(path)
	{
	}
};

struct fs_path_t
{
	fs_path path;
};


[[nodiscard]] vsm::result<fs_entry_info> get_fs_entry_info(fs_path path);

[[nodiscard]] vsm::result<size_t> get_absolute_path(fs_path const& path);


[[nodiscard]] bool is_null_device_path(any_path_view path);


#if 0 // TODO: Move to a separate header
namespace _nothrow {

[[nodiscard]] vsm::result<void> delete_file(fs_path const& path);

[[nodiscard]] vsm::result<void> unlink_file(fs_path const& path);

[[nodiscard]] vsm::result<void> rename_file(fs_path const& old_path, fs_path const& new_path);

[[nodiscard]] vsm::result<void> link_file(fs_path const& link_path, fs_path const& file_path);

[[nodiscard]] vsm::result<size_t> get_symbolic_link_path(
	fs_path const& link_path,
	any_path_buffer buffer);

template<typename Path = path>
[[nodiscard]] vsm::result<Path> get_symbolic_link_path(fs_path const& link_path)
{
	vsm::result<Path> r(vsm::result_value);
	if (auto const r2 = _nothrow::get_symbolic_link_path(link_path, *r); !r2)
	{
		r = vsm::unexpected(r2.error());
	}
	return r;
}

[[nodiscard]] vsm::result<void> create_file_symbolic_link(
	fs_path const& link_path,
	any_path_view target_path);

[[nodiscard]] vsm::result<void> create_directory_symbolic_link(
	fs_path const& link_path,
	any_path_view target_path);

} // namespace _nothrow
#endif

} // namespace allio::detail
