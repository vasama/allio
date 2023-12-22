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
	none                                = 1,

	read_data                           = 1 | 1 << 1,
	write_data                          = 1 | 1 << 2,

	read_attributes                     = 1 | 1 << 3,
	write_attributes                    = 1 | 1 << 4,

	read                                = read_data | read_attributes,
	write                               = write_data | write_attributes,
	read_write                          = read | write,

	all                                 = read_write,
};
vsm_flag_enum(file_mode);

enum class file_opening : uint8_t
{
	open_existing = 1,
	create_only,
	open_or_create,
	truncate_existing,
	replace_existing,
};

/// @brief Controls sharing of the file by other handles.
/// @note File sharing restrictions may not be available on all platforms.
///       For maximum portability, do not specify file sharing restrictions.
enum class file_sharing : uint8_t
{
	unlink                              = 1 | 1 << 1,
	read                                = 1 | 1 << 2,
	write                               = 1 | 1 << 3,

	all = unlink | read | write,
};
vsm_flag_enum(file_sharing);

enum class file_caching : uint8_t
{
	none = 1,
	only_metadata,
	reads,
	reads_and_metadata,
	all,
	safety_barriers,
	temporary,
};

enum class file_options : uint16_t
{
	none                                        = 1,
	unlink_on_first_close                       = 1 | 1 << 1,
	disable_safety_barriers                     = 1 | 1 << 2,
	disable_safety_unlinks                      = 1 | 1 << 3,
	disable_prefetching                         = 1 | 1 << 4,
	maximum_prefetching                         = 1 | 1 << 5,

#if vsm_os_win32
#	define allio_detail_windows_flag(x) 1 | 1 << x
#else
#	define allio_detail_windows_flag(x) 0
#endif

	windows_disable_unlink_emulation            = allio_detail_windows_flag(6),
	windows_disable_sparse_file_creation        = allio_detail_windows_flag(7),

#undef allio_detail_windows_flag
};
vsm_flag_enum(file_options);

enum class path_kind : uint8_t
{
#if vsm_os_win32
#	define allio_detail_windows_flag(x) 1 << x
#else
#	define allio_detail_windows_flag(x) 0
#endif

	windows_nt                          = allio_detail_windows_flag(0),
	windows_guid                        = allio_detail_windows_flag(1),
	windows_dos                         = allio_detail_windows_flag(2),

#undef allio_detail_windows_flag

	any = windows_nt | windows_guid | windows_dos,
};
vsm_flag_enum(path_kind);

enum class open_options : uint8_t
{
	temporary                           = 1 << 0,
	unique_name                         = 1 << 1,
	anonymous                           = 1 << 2,
};
vsm_flag_enum(open_options);


struct path_kind_t
{
	path_kind kind = path_kind::any;

	friend void tag_invoke(set_argument_t, path_kind_t& args, path_kind const kind)
	{
		args.kind = kind;
	}
};

namespace fs_io {

struct open_t
{
	using operation_concept = producer_t;

	struct params_type : io_flags_t
	{
		file_mode mode = {};
		file_options options = {};
		file_opening opening = {};
		file_sharing sharing = {};
		file_caching caching = {};
		open_options special = {};
		fs_path path = {};

		friend void tag_invoke(set_argument_t, params_type& args, file_mode const value)
		{
			args.mode = value;
		}
	
		friend void tag_invoke(set_argument_t, params_type& args, file_opening const value)
		{
			args.opening = value;
		}
	
		friend void tag_invoke(set_argument_t, params_type& args, file_sharing const value)
		{
			args.sharing = value;
		}
	
		friend void tag_invoke(set_argument_t, params_type& args, file_caching const value)
		{
			args.caching = value;
		}
	
		friend void tag_invoke(set_argument_t, params_type& args, file_options const value)
		{
			args.options = value;
		}
	
		friend void tag_invoke(set_argument_t, params_type& args, open_options const value)
		{
			args.special = value;
		}
	
		friend void tag_invoke(set_argument_t, params_type& args, fs_path const& value)
		{
			args.path = value;
		}
	};

	using result_type = void;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<open_t>,
		native_handle<Object>& h,
		io_parameters_t<Object, open_t> const& a)
		requires requires { Object::open(h, a); }
	{
		return Object::open(h, a);
	}
};

struct get_current_path_t
{
	using operation_concept = void;

	struct params_type
	{
		any_path_buffer buffer;
		path_kind kind = path_kind::any;

		friend void tag_invoke(set_argument_t, params_type& args, path_kind const value)
		{
			args.kind = value;
		}
	};

	using result_type = size_t;

	template<object Object>
	friend vsm::result<size_t> tag_invoke(
		blocking_io_t<get_current_path_t>,
		native_handle<Object> const& h,
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

	using open_t = fs_io::open_t;
	using get_current_path_t = fs_io::get_current_path_t;

	using operations = type_list_append
	<
		base_type::operations
		, open_t
		, get_current_path_t
	>;

	static vsm::result<size_t> get_current_path(
		native_handle<fs_object_t> const& h,
		io_parameters_t<fs_object_t, get_current_path_t> const& a);

	template<typename Handle, typename Traits>
	struct facade : base_type::facade<Handle, Traits>
	{
		[[nodiscard]] auto read_current_path(any_path_buffer const buffer, auto&&... args) const
		{
			auto r = _read_current_path(buffer, vsm_forward(args)...);

			if constexpr (Traits::has_transform_result)
			{
				return Traits::transform_result(vsm_move(r));
			}
			else
			{
				return r;
			}
		}

		template<typename Path = path>
		[[nodiscard]] auto get_current_path(auto&&... args) const
		{
			auto r = _get_current_path<Path>(vsm_forward(args)...);

			if constexpr (Traits::has_transform_result)
			{
				return Traits::transform_result(vsm_move(r));
			}
			else
			{
				return r;
			}
		}

	private:
		vsm::result<size_t> _read_current_path(any_path_buffer const buffer, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, get_current_path_t>{};
			a.buffer = buffer;
			(set_argument(a, vsm_forward(args)), ...);

			return blocking_io<typename Handle::object_type, get_current_path_t>(
					static_cast<Handle const&>(*this),
					a);
		}

		template<typename Path>
		vsm::result<Path> _get_current_path(auto&&... args) const
		{
			vsm::result<Path> r(vsm::result_value);
			vsm_try_void(_read_current_path(any_path_buffer(*r), vsm_forward(args)...));
			return r;
		}
	};
};

} // namespace allio::detail
