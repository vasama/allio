#pragma once

#include <allio/multiplexer.hpp>
#include <allio/platform_handle.hpp>
#include <allio/path.hpp>
#include <allio/path_view.hpp>
#include <allio/type_list.hpp>

#include <span>

namespace allio {

enum class file_mode : uint8_t
{
	none                                = 0b0010,
	attr_read                           = 0b0100,
	attr_write                          = 0b0101,
	read                                = 0b0110,
	write                               = 0b0111,
	append                              = 0b1001,
};

enum class file_creation : uint8_t
{
	open_existing,
	create_only,
	open_or_create,
	truncate_existing,
	replace_existing,
};

enum class file_caching : uint8_t
{
};

enum class file_flags : uint32_t
{
	none                                = 0,
	unlink_on_first_close               = 1 << 0,
	disable_safety_barriers             = 1 << 1,
	disable_safety_unlinks              = 1 << 2,
	disable_prefetching                 = 1 << 3,
	maximum_prefetching                 = 1 << 4,

#if _WIN32
#	define allio_detail_WIN_FILE_FLAG(x) 1 << x
#else
#	define allio_detail_WIN_FILE_FLAG(x) 0
#endif

	win_disable_unlink_emulation        = allio_detail_WIN_FILE_FLAG(24),
	win_disable_sparse_file_creation    = allio_detail_WIN_FILE_FLAG(25),

#undef allio_detail_WIN_FILE_FLAG
};

struct file_parameters
{
	flags handle_flags = {};
	file_mode mode = file_mode::read;
	file_creation creation = file_creation::open_existing;
	file_caching caching = {};
	file_flags flags = {};
};

enum class path_kind : uint32_t
{
	any                         = 0,

	windows_nt                  = 1 << 0,
	windows_guid                = 1 << 1,
	windows_dos                 = 1 << 2,
};

class filesystem_handle;

namespace detail {

template<typename Char>
result<basic_path<Char>> get_current_path(filesystem_handle const& handle, path_kind kind);

template<typename Char>
result<basic_path_view<Char>> copy_current_path(filesystem_handle const& handle, std::span<Char> buffer, path_kind kind);

} // namespace detail

namespace io {

struct open_file;

template<>
struct parameters<open_file>
{
	using handle_type = filesystem_handle;
	using result_type = void;

	filesystem_handle const* base;
	path_view path;
	file_parameters args;
};

template<typename Char>
struct get_current_path;

template<typename Char>
struct parameters<get_current_path<Char>>
{
	using handle_type = filesystem_handle;
	using result_type = void;

	path_kind kind;
	basic_path<Char>* path;
};

template<typename Char>
struct copy_current_path;

template<typename Char>
struct parameters<copy_current_path<Char>> : async_operation_parameters
{
	using handle_type = filesystem_handle;
	using result_type = void;

	path_kind kind;
	std::span<Char> buffer;
};

} // namespace io

class filesystem_handle : public platform_handle
{
public:
	using async_operations = type_list_cat<
		platform_handle::async_operations,
		type_list<
			io::open_file,
			io::get_current_path<char>,
			io::copy_current_path<char>
		>
	>;

	template<typename Char = char>
	result<basic_path<Char>> get_current_path(path_kind const kind = path_kind::any) const
	{
		return detail::get_current_path<Char>(*this, kind);
	}

	template<typename Char = char>
	result<basic_path_view<Char>> copy_current_path(std::span<Char> const buffer, path_kind const kind = path_kind::any) const
	{
		return detail::copy_current_path<Char>(*this, buffer, kind);
	}

protected:
	using platform_handle::platform_handle;

	filesystem_handle() = default;
	filesystem_handle(filesystem_handle&&) = default;
	filesystem_handle& operator=(filesystem_handle&&) = default;
	~filesystem_handle() = default;
};

} // namespace allio
