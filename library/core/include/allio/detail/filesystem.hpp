#pragma once

#include <allio/any_path.hpp>
#include <allio/any_path_buffer.hpp>
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


enum class path_format : uint8_t {};

class path_formats
{
	uint8_t m_bits = 0;

public:
	class iterator
	{
		uint8_t m_bits;

	public:
		using value_type = path_format;
		using difference_type = ptrdiff_t;

		iterator() = default;

		[[nodiscard]] path_format operator*() const
		{
			return static_cast<path_format>(m_bits & 0b11);
		}

		iterator& operator++() &
		{
			m_bits >>= 2;
			return *this;
		}

		[[nodiscard]] iterator operator++(int) &
		{
			auto r = *this;
			m_bits >>= 2;
			return r;
		}

		[[nodiscard]] friend bool operator==(iterator const&, iterator const&) = default;

	private:
		explicit iterator(uint8_t const bits)
			: m_bits(bits)
		{
		}

		friend path_formats;
	};


	path_formats() = default;

	constexpr path_formats(path_format const format)
		: m_bits(static_cast<uint8_t>(format))
	{
	}


	[[nodiscard]] bool empty() const
	{
		return m_bits == 0;
	}

	[[nodiscard]] size_t size() const
	{
		uint8_t const _0 = m_bits & 0b00000011;
		uint8_t const _1 = m_bits & 0b00001100;
		uint8_t const _2 = m_bits & 0b00110000;
		uint8_t const _3 = m_bits & 0b11000000;

		return (_0 != 0) + (_1 != 0) + (_2 != 0) + (_3 != 0);
	}


	[[nodiscard]] iterator begin() const
	{
		return iterator(m_bits);
	}

	[[nodiscard]] iterator end() const
	{
		return iterator(0);
	}


	[[nodiscard]] friend path_formats operator|(path_formats const self, path_format const format)
	{
		uint8_t const x = static_cast<uint8_t>(format);

		if (x == 0 || self.has_bit_pair(x))
		{
			return self;
		}

		return path_formats(self.m_bits | x << self.size() * 2);
	}

	[[nodiscard]] friend path_formats operator|(path_format const format, path_formats const self)
	{
		uint8_t const x = static_cast<uint8_t>(format);

		if (x == 0 || self.has_bit_pair(x))
		{
			return self;
		}

		return path_formats(x | self.m_bits << 2);
	}


	static path_formats const any;

private:
	explicit constexpr path_formats(uint8_t const bits)
		: m_bits(bits)
	{
	}

	bool has_bit_pair(uint8_t const x) const
	{
		unsigned const z = m_bits ^ (x * 0b01010101);
		return (z - 0b01010101) & (~z & 0b10101010);
	}
};

[[nodiscard]] inline path_formats operator|(path_format const lhs, path_format const rhs)
{
	return path_formats(lhs) | rhs;
}

inline constexpr path_formats path_formats::any = path_formats();


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


vsm::result<size_t> canonical_path(fs_path const& path, any_path_buffer buffer);
vsm::result<size_t> weakly_canonical_path(fs_path const& path, any_path_buffer buffer);
vsm::result<size_t> relative_path(fs_path const& path, fs_path const& base, any_path_buffer buffer);

vsm::result<size_t> proximate_path(
	fs_path const& path,
	fs_path const& base,
	any_path_buffer buffer);

template<typename Char, typename Encoding>
vsm::result<basic_path_view<Char, Encoding>> canonical_path(
	basic_path_view<Char, Encoding> path,
	path_buffer<Char, Encoding> buffer);

template<typename Char, typename Encoding>
vsm::result<basic_path_view<Char, Encoding>> weakly_canonical_path(
	basic_path_view<Char, Encoding> path,
	path_buffer<Char, Encoding> buffer);

template<typename Char, typename Encoding>
vsm::result<basic_path_view<Char, Encoding>> relative_path(
	basic_path_view<Char, Encoding> path,
	fs_path const& base,
	path_buffer<Char, Encoding> buffer);

template<typename Char, typename Encoding>
vsm::result<basic_path_view<Char, Encoding>> proximate_path(
	basic_path_view<Char, Encoding> path,
	fs_path const& base,
	path_buffer<Char, Encoding> buffer);


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
