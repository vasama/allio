#pragma once

#include <allio/any_path.hpp>
#include <allio/any_path_buffer.hpp>
#include <allio/detail/filesystem.hpp>
#include <allio/detail/handles/platform_object.hpp>
#include <allio/path.hpp>
#include <allio/path_view.hpp>

namespace allio::detail {

enum class file_mode : uint8_t
{
	none                                = 0,

	read_data                           = 1 << 0,
	write_data                          = 1 << 1,

	read_attributes                     = 1 << 2,
	write_attributes                    = 1 << 3,

	read                                = read_data | read_attributes,
	write                               = write_data | write_attributes,
	read_write                          = read | write,

	all                                 = read_write,
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
///       For maximum portability, specify no file sharing restrictions (all).
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
	none                                = 0,
};

enum class file_flags : uint32_t
{
	none                                = 0,
	unlink_on_first_close               = 1 << 0,
	disable_safety_barriers             = 1 << 1,
	disable_safety_unlinks              = 1 << 2,
	disable_prefetching                 = 1 << 3,
	maximum_prefetching                 = 1 << 4,

#if vsm_os_win32
#	define allio_detail_win32_flag(x) 1 << x
#else
#	define allio_detail_win32_flag(x) 0
#endif

	win_disable_unlink_emulation        = allio_detail_win32_flag(24),
	win_disable_sparse_file_creation    = allio_detail_win32_flag(25),

#undef allio_detail_win32_flag
};
vsm_flag_enum(file_flags);

enum class path_kind : uint8_t
{
#if vsm_os_win32
#	define allio_detail_win32_flag(x) 1 << x
#else
#	define allio_detail_win32_flag(x) 0
#endif

	windows_nt                          = allio_detail_win32_flag(0),
	windows_guid                        = allio_detail_win32_flag(1),
	windows_dos                         = allio_detail_win32_flag(2),

#undef allio_detail_win32_flag

	any = windows_nt | windows_guid | windows_dos,
};
vsm_flag_enum(path_kind);


struct file_mode_t
{
	std::optional<file_mode> mode;

	friend void tag_invoke(set_argument_t, file_mode_t& args, file_mode const mode)
	{
		args.mode = mode;
	}
};

struct file_creation_t
{
	std::optional<file_creation> creation;

	friend void tag_invoke(set_argument_t, file_creation_t& args, file_creation const creation)
	{
		args.creation = creation;
	}
};

struct file_sharing_t
{
	file_sharing sharing = file_sharing::all;

	friend void tag_invoke(set_argument_t, file_sharing_t& args, file_sharing const sharing)
	{
		args.sharing = sharing;
	}
};

struct file_caching_t
{
	file_caching caching = file_caching::none;

	friend void tag_invoke(set_argument_t, file_caching_t& args, file_caching const caching)
	{
		args.caching = caching;
	}
};

struct file_flags_t
{
	file_flags flags = file_flags::none;

	friend void tag_invoke(set_argument_t, file_flags_t& args, file_flags const flags)
	{
		args.flags = flags;
	}
};

struct path_kind_t
{
	detail::path_kind path_kind = detail::path_kind::any;

	friend void tag_invoke(set_argument_t, path_kind_t& args, detail::path_kind const path_kind)
	{
		args.path_kind = path_kind;
	}
};


namespace fs_io {

struct open_t
{
	using operation_concept = producer_t;
	using required_params_type = fs_path_t;
	using optional_params_type = parameters_t
	<
		inheritable_t,
		file_mode_t,
		file_creation_t,
		file_sharing_t,
		file_caching_t,
		file_flags_t
	>;
	using result_type = void;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, open_t>,
		typename Object::native_type& h,
		io_parameters_t<Object, open_t> const& a)
		requires requires { Object::open(h, a); }
	{
		return Object::open(h, a);
	}
};

struct get_current_path_t
{
	using operation_concept = void;
	struct required_params_type
	{
		any_path_buffer buffer;
	};
	using optional_params_type = path_kind_t;
	using result_type = size_t;

	template<object Object>
	friend vsm::result<size_t> tag_invoke(
		blocking_io_t<Object, get_current_path_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, get_current_path_t> const& a)
		requires requires { Object::open(h, a); }
	{
		return Object::get_current_path(h, a);
	}
};

} // namespace fs_io

struct fs_object_t : platform_object_t
{
	using base_type = platform_object_t;

	allio_handle_flags
	(
		readable,
		writable,
	);

	struct native_type : base_type::native_type
	{
		detail::file_flags file_flags;
	};

	using open_t = fs_io::open_t;
	using get_current_path_t = fs_io::get_current_path_t;

	using operations = type_list_append
	<
		base_type::operations
		, open_t
		, get_current_path_t
	>;

	static vsm::result<size_t> get_current_path(
		native_type const& h,
		io_parameters_t<fs_object_t, get_current_path_t> const& a);

	template<typename Handle>
	struct abstract_interface : base_type::abstract_interface<Handle>
	{
		[[nodiscard]] vsm::result<size_t> get_current_path(any_path_buffer const buffer, auto&&... args)
		{
			return blocking_io<typename Handle::object_type, get_current_path_t>(
				static_cast<Handle const&>(*this),
				make_io_args<typename Handle::object_type, get_current_path_t>(buffer)(vsm_forward(args)...));
		}

		template<typename Path = path>
		[[nodiscard]] vsm::result<Path> get_current_path(auto&&... args)
		{
			vsm::result<Path> r(vsm::result_value);
			vsm_try_discard(get_current_path(any_path_buffer(*r), vsm_forward(args)...));
			return r;
		}
	};
};

} // namespace allio::detail
