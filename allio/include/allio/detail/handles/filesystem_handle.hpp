#pragma once

#include <allio/any_path.hpp>
#include <allio/any_path_buffer.hpp>
#include <allio/detail/handles/platform_handle.hpp>
#include <allio/filesystem.hpp>
#include <allio/any_path_buffer.hpp>
#include <allio/path.hpp>
#include <allio/path_view.hpp>

namespace allio::detail {

class filesystem_handle;


enum class file_mode : uint8_t
{
	none                                = 1 << 0,
	read_attributes                     = 1 << 1,
	write_attributes                    = 1 << 2,
	read                                = 1 << 3,
	write                               = 1 << 4,

	read_write = read | write,
};
vsm_flag_enum(file_mode);

enum class file_creation : uint8_t
{
	open_existing,
	create_only,
	open_or_create,
	truncate_existing,
	replace_existing,
};

/// @brief Controls sharing of the file by other handles.
/// @note File sharing restrictions may not be available on all platforms.
enum class file_sharing : uint8_t
{
	unlink                              = 1 << 0,
	read                                = 1 << 1,
	write                               = 1 << 2,

	all = unlink | read | write,
};
vsm_flag_enum(file_sharing);

enum class file_caching : uint8_t
{
	none                                                    = 0,
};

enum class file_flags : uint32_t
{
	none                                                    = 0,
	unlink_on_first_close                                   = 1 << 0,
	disable_safety_barriers                                 = 1 << 1,
	disable_safety_unlinks                                  = 1 << 2,
	disable_prefetching                                     = 1 << 3,
	maximum_prefetching                                     = 1 << 4,

#if vsm_os_win32
#	define allio_detail_win32_flag(x) 1 << x
#else
#	define allio_detail_win32_flag(x) 0
#endif

	win_disable_unlink_emulation                            = allio_detail_win32_flag(24),
	win_disable_sparse_file_creation                        = allio_detail_win32_flag(25),

#undef allio_detail_win32_flag
};

enum class path_kind : uint32_t
{
#if vsm_os_win32
#	define allio_detail_win32_flag(x) 1 << x
#else
#	define allio_detail_win32_flag(x) 0
#endif

	win_nt                                                  = allio_detail_win32_flag(0),
	win_guid                                                = allio_detail_win32_flag(1),
	win_dos                                                 = allio_detail_win32_flag(2),

#undef allio_detail_win32_flag

	any = win_nt | win_guid | win_dos,
};
vsm_flag_enum(path_kind);

struct path_descriptor
{
	filesystem_handle const* base;
	any_path_view path;

	template<std::convertible_to<any_path_view> Path>
	path_descriptor(Path const& path)
		: base(nullptr)
		, path(path)
	{
	}

	template<std::convertible_to<any_path_view> Path>
	path_descriptor(filesystem_handle const& base, Path const& path)
		: base(&base)
		, path(path)
	{
	}
};

inline path_descriptor at(filesystem_handle const& location, any_path_view const path)
{
	return path_descriptor(location, path);
}


struct file_mode_t
{
	detail::file_mode mode = detail::file_mode::read_write;
};

struct file_creation_t
{
	detail::file_creation creation = detail::file_creation::open_existing;
};

struct file_sharing_t
{
	detail::file_sharing sharing = detail::file_sharing::all;
};

struct file_caching_t
{
	detail::file_caching caching = detail::file_caching::none;
};

struct file_flags_t
{
	detail::file_flags file_flags = detail::file_flags::none;
};

struct path_kind_t
{
	detail::path_kind kind = detail::path_kind::any;
};


class filesystem_handle : public platform_handle
{
protected:
	using base_type = platform_handle;

public:
	struct open_t
	{
		using handle_type = filesystem_handle;
		using result_type = void;
		using required_params_type = no_parameters_t;
		using optional_params_type = parameters_t<
			file_mode_t,
			file_creation_t,
			file_sharing_t,
			file_caching_t,
			file_flags_t>;
	};


	struct get_current_path_t
	{
		using handle_type = filesystem_handle const;
		using result_type = size_t;

		struct required_params_type
		{
			any_path_buffer buffer;
		};

		using optional_params_type = path_kind_t;
	};

	vsm::result<size_t> get_current_path(any_path_buffer const output, auto&&... args) const
	{
		return do_blocking_io(
			*this,
			io_args<get_current_path_t>(output)(vsm_forward(args)...));
	}

	template<typename Path = path>
	vsm::result<Path> get_current_path(auto&&... args) const
	{
		vsm::result<Path> r(vsm::result_value);
		vsm_try_void(do_blocking_io(
			*this,
			io_args<get_current_path_t>(*r)(vsm_forward(args)...)));
		return r;
	}


	using asynchronous_operations = type_list_cat<
		base_type::asynchronous_operations,
		type_list<open_t>
	>;

protected:
	allio_detail_default_lifetime(filesystem_handle);

	using platform_handle::platform_handle;

protected:
	static vsm::result<void> do_blocking_io(
		filesystem_handle const& h,
		io_result_ref_t<get_current_path_t> r,
		io_parameters_t<get_current_path_t> const& args);
};


vsm::result<file_attributes> query_file_attributes(path_descriptor path);

} // namespace allio::detail
