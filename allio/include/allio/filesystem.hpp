#pragma once

#include <vsm/flags.hpp>

#include <bit>
#include <compare>

#include <cstddef>
#include <cstdint>

namespace allio {

using file_size = uint64_t;

//TODO: Use std::chrono
enum class file_time : uint64_t {};


enum class file_id_64 : uint64_t {};

class file_id_128
{
	static constexpr size_t l = std::endian::native == std::endian::big;
	static constexpr size_t h = std::endian::native != std::endian::big;

	file_id_64 m_data[2];

public:
	file_id_128() = default;

	file_id_128(file_id_64 const id)
	{
		m_data[l] = id;
		m_data[h] = {};
	}

	explicit operator file_id_64() const
	{
		return m_data[l];
	}

	friend auto operator<=>(file_id_128 const&, file_id_128 const&) = default;
};


enum class file_kind : uint8_t
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

class file_permissions
{
};

struct file_attributes
{
	file_kind kind;
	file_permissions permissions;

	file_id_64 device_id;
	file_id_64 node_id;

	file_size file_end_offset;
	file_size hard_link_count;

	file_time last_data_access_time;
	file_time last_data_modify_time;
	file_time last_node_modify_time;
};


//vsm::result<file_attributes> query_file_attributes(path_descriptor path);

} // namespace allio
