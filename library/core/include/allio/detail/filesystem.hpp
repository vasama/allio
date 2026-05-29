#pragma once

#include <allio/any_byte_buffer.hpp>
#include <allio/any_path.hpp>
#include <allio/any_path_buffer.hpp>
#include <allio/detail/basic_storage.hpp>
#include <allio/detail/object_concepts.hpp>

#include <vsm/flags.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>

#include <chrono>
#include <compare>
#include <concepts>

#include <cstddef>
#include <cstdint>

namespace allio::detail {

struct fs_object_t;


using fs_size = uint64_t;



enum class fs_type : uint_least8_t
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

enum class fs_dev_id : uint64_t {};
enum class fs_ino_id : uint64_t {};


/// @brief Portable filesystem clock. Represented as nanoseconds since Jan 1 1970 UTC.
/// @note The clock is not steady and as such is not suitable for interval measurements.
struct fs_clock
{
	using rep = int64_t;
	using period = std::nano;
	using duration = std::chrono::duration<rep, period>;
	using time_point = std::chrono::time_point<fs_clock>;

	static constexpr bool is_steady = false;

	[[nodiscard]] static time_point now() noexcept;
};

using fs_duration = fs_clock::duration;
using fs_time_point = fs_clock::time_point;


struct fs_io_limits
{
	uint32_t minimum_buffer_alignment;

	uint32_t minimum_reading_alignment;
	uint32_t optimal_reading_alignment;

	uint32_t minimum_writing_alignment;
	uint32_t optimal_writing_alignment;

	uint32_t minimum_atomic_write_size;
	uint32_t optimal_atomic_write_size;
	uint32_t maximum_atomic_write_size;
};


enum class fs_info_mask : uint_least16_t
{
	none                        = 0,

	type                        = 1 << 1,

	dev_id                      = 1 << 2,
	ino_id                      = 1 << 3,

	link_count                  = 1 << 4,
	max_extent                  = 1 << 5,

	create_time                 = 1 << 6,
	data_access_time            = 1 << 7,
	data_change_time            = 1 << 8,
	attr_change_time            = 1 << 9,

	io_limits                   = 1 << 10,

	/// @brief The set of widely supported filesystem timestamps.
	standard_timestamps = data_access_time | data_change_time | attr_change_time,

	/// @brief The full set of filesystem timestamps supported by the library.
	timestamps = create_time | standard_timestamps,

	/// @brief The standard file information set (POSIX stat).
	standard = type | dev_id | ino_id | link_count | max_extent | standard_timestamps,
};
vsm_flag_enum(fs_info_mask);


[[nodiscard]] fs_type _fs_info_type(void const* info);

[[nodiscard]] fs_dev_id _fs_info_dev_id(void const* info);
[[nodiscard]] fs_ino_id _fs_info_ino_id(void const* info);

[[nodiscard]] fs_size _fs_info_link_count(void const* info);
[[nodiscard]] fs_size _fs_info_max_extent(void const* info);

[[nodiscard]] fs_time_point _fs_info_create_time(void const* info);
[[nodiscard]] fs_time_point _fs_info_data_access_time(void const* info);
[[nodiscard]] fs_time_point _fs_info_data_change_time(void const* info);
[[nodiscard]] fs_time_point _fs_info_attr_change_time(void const* info);

[[nodiscard]] fs_io_limits _fs_info_io_limits(void const* info);


class fs_info_view
{
	fs_info_mask m_mask = fs_info_mask::none;
	void const* m_info;

public:
	fs_info_view() = default;

	explicit fs_info_view(void const* const info, fs_info_mask const mask) noexcept
		: m_mask(mask)
		, m_info(info)
	{
	}

	[[nodiscard]] fs_info_mask mask() const noexcept
	{
		return m_mask;
	}

	[[nodiscard]] bool contains(fs_info_mask const mask) const noexcept
	{
		return vsm::all_flags(m_mask, mask);
	}

	[[nodiscard]] explicit operator bool() const noexcept
	{
		return m_mask != fs_info_mask::none;
	}


	[[nodiscard]] fs_type type() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::type)); //PRECONDITION
		return detail::_fs_info_type(m_info);
	}

	[[nodiscard]] fs_dev_id dev_id() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::dev_id)); //PRECONDITION
		return detail::_fs_info_dev_id(m_info);
	}

	[[nodiscard]] fs_ino_id ino_id() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::ino_id)); //PRECONDITION
		return detail::_fs_info_ino_id(m_info);
	}

	[[nodiscard]] fs_size link_count() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::link_count)); //PRECONDITION
		return detail::_fs_info_link_count(m_info);
	}

	[[nodiscard]] fs_size max_extent() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::max_extent)); //PRECONDITION
		return detail::_fs_info_max_extent(m_info);
	}

	[[nodiscard]] fs_time_point create_time() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::create_time)); //PRECONDITION
		return detail::_fs_info_create_time(m_info);
	}

	[[nodiscard]] fs_time_point data_access_time() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::data_access_time)); //PRECONDITION
		return detail::_fs_info_data_access_time(m_info);
	}

	[[nodiscard]] fs_time_point data_change_time() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::data_change_time)); //PRECONDITION
		return detail::_fs_info_data_change_time(m_info);
	}

	[[nodiscard]] fs_time_point attr_change_time() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::attr_change_time)); //PRECONDITION
		return detail::_fs_info_attr_change_time(m_info);
	}

	[[nodiscard]] fs_io_limits io_limits() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::io_limits)); //PRECONDITION
		return detail::_fs_info_io_limits(m_info);
	}
};

class fs_standard_info
{
	fs_info_mask m_mask = fs_info_mask::none;

	fs_type m_type;

	fs_dev_id m_dev_id;
	fs_ino_id m_ino_id;

	fs_size m_link_count;
	fs_size m_max_extent;

	fs_time_point m_data_access_time;
	fs_time_point m_data_change_time;
	fs_time_point m_attr_change_time;

public:
	fs_standard_info() = default;

	fs_standard_info(fs_info_view const view) noexcept;

	[[nodiscard]] fs_info_mask mask() const
	{
		return m_mask;
	}

	[[nodiscard]] bool contains(fs_info_mask const mask) const noexcept
	{
		return vsm::all_flags(m_mask, mask);
	}

	[[nodiscard]] fs_type type() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::type)); //PRECONDITION
		return m_type;
	}

	[[nodiscard]] fs_dev_id dev_id() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::dev_id)); //PRECONDITION
		return m_dev_id;
	}

	[[nodiscard]] fs_ino_id ino_id() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::ino_id)); //PRECONDITION
		return m_ino_id;
	}

	[[nodiscard]] fs_size link_count() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::link_count)); //PRECONDITION
		return m_link_count;
	}

	[[nodiscard]] fs_size max_extent() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::max_extent)); //PRECONDITION
		return m_max_extent;
	}

	[[nodiscard]] fs_time_point const& data_access_time() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::data_access_time)); //PRECONDITION
		return m_data_access_time;
	}

	[[nodiscard]] fs_time_point const& data_change_time() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::data_change_time)); //PRECONDITION
		return m_data_change_time;
	}

	[[nodiscard]] fs_time_point const& attr_change_time() const
	{
		vsm_assert(vsm::any_flags(m_mask, fs_info_mask::attr_change_time)); //PRECONDITION
		return m_attr_change_time;
	}
};


template<typename Information>
struct fs_info_traits
{
};

template<>
struct fs_info_traits<fs_io_limits>
{
	static constexpr fs_info_mask info_mask = fs_info_mask::io_limits;

	static fs_io_limits from_info(fs_info_view const info) noexcept
	{
		return info.io_limits();
	}
};

template<>
struct fs_info_traits<fs_standard_info>
{
	static constexpr fs_info_mask info_mask = fs_info_mask::standard;

	static fs_standard_info from_info(fs_info_view const info) noexcept
	{
		return fs_standard_info(info);
	}
};


template<std::same_as<fs_info_mask> auto>
inline constexpr bool match_fs_info_mask = true;

template<typename Information>
concept fs_info_like = requires (fs_info_view const& info)
{
	requires match_fs_info_mask<fs_info_traits<Information>::info_mask>;

	{
		fs_info_traits<Information>::from_info(info)
	} noexcept -> vsm::nothrow_assignable_to_lvalue<Information>;
};


class fs_info_ref
{
	using assign_fs_info_t = void(void* object, fs_info_view info);

	assign_fs_info_t* m_assign;
	void* m_object;

public:
	template<fs_info_like Information>
	fs_info_ref(Information& information)
		: m_assign(_assign_fs_info<Information>)
		, m_object(std::addressof(information))
	{
	}

	void assign(fs_info_view const view) const noexcept
	{
		m_assign(m_object, view);
	}

private:
	explicit fs_info_ref(void* const object, assign_fs_info_t* const assign) noexcept
		: m_assign(assign)
		, m_object(object)
	{
	}

	template<typename Information>
	static void _assign_fs_info(void* const object, fs_info_view const view)
	{
		*static_cast<Information*>(object) = fs_info_traits<Information>::from_info(view);
	}

	friend class fs_info_buffer_ref;
};

class fs_info_buffer_ref : any_byte_buffer_ref_base
{
public:
	using any_byte_buffer_ref_base::any_byte_buffer_ref_base;

	fs_info_buffer_ref(fs_info_ref const ref) noexcept
		: any_byte_buffer_ref_base(ref.m_object, ref.m_assign)
	{
	}

	template<fs_info_like Information>
	fs_info_buffer_ref(Information& information)
		: fs_info_buffer_ref(fs_info_ref(information))
	{
	}

	[[nodiscard]] bool is_byte_buffer() const noexcept
	{
		return !is_user_defined();
	}

	using any_byte_buffer_ref_base::as_byte_buffer;

	[[nodiscard]] bool is_fs_info_ref() const noexcept
	{
		return is_user_defined();
	}

	[[nodiscard]] fs_info_ref as_fs_info_ref() const
	{
		return fs_info_ref(
			object_pointer(),
			static_cast<fs_info_ref::assign_fs_info_t*>(function_pointer()));
	}
};


// Enough to store the currently available information in any supported platform.
inline constexpr size_t fs_info_storage_size = 256;
inline constexpr size_t fs_info_alignment = alignof(uint64_t);

using fs_info_buffer = basic_storage<
	std::byte,
	fs_info_storage_size,
	fs_info_alignment>;


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

		return static_cast<size_t>((_0 != 0) + (_1 != 0) + (_2 != 0) + (_3 != 0));
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

		return path_formats(static_cast<uint8_t>(self.m_bits | x << self.size() * 2));
	}

	[[nodiscard]] friend path_formats operator|(path_format const format, path_formats const self)
	{
		uint8_t const x = static_cast<uint8_t>(format);

		if (x == 0 || self.has_bit_pair(x))
		{
			return self;
		}

		return path_formats(static_cast<uint8_t>(x | self.m_bits << 2));
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
