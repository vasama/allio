#pragma once

#include <allio/any_path.hpp>
#include <allio/any_path_buffer.hpp>
#include <allio/detail/handles/platform_object.hpp>
#include <allio/filesystem.hpp>
#include <allio/any_path_buffer.hpp>
#include <allio/path.hpp>
#include <allio/path_view.hpp>

namespace allio::detail {

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


struct fs_object_t;

struct path_descriptor
{
	native_platform_handle base;
	any_path_view path;

	template<std::convertible_to<any_path_view> Path>
	path_descriptor(Path const& path)
		: base(native_platform_handle::null)
		, path(path)
	{
	}

	template<std::derived_from<fs_object_t> FsObject, std::convertible_to<any_path_view> Path>
	path_descriptor(detail::abstract_handle<FsObject> const& base, Path const& path)
		: base(base.native().platform_object_t::native_type::platform_handle)
		, path(path)
	{
	}
};


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
	detail::path_kind path_kind = detail::path_kind::any;
};


struct fs_object_t : platform_object_t
{
	using base_type = platform_object_t;

	struct open_t
	{
		using operation_concept = producer_t;

		struct required_params_type
		{
			path_descriptor path;
		};

		using optional_params_type = parameters_t
		<
			file_mode_t,
			file_creation_t,
			file_sharing_t,
			file_caching_t,
			file_flags_t
		>;
	};

	struct get_current_path_t
	{
		using result_type = size_t;
		
		struct required_params_type
		{
			any_path_buffer buffer;
		};
		
		using optional_params_type = path_kind_t;
	};

	using operations = type_list_append
	<
		base_type::operations
		, open_t
		, get_current_path_t
	>;

	template<typename H>
	struct abstract_interface : base_type::abstract_interface<H>
	{
		[[nodiscard]] vsm::result<size_t> get_current_path(any_path_buffer const buffer, auto&&... args)
		{
			return blocking_io<get_current_path_t>(
				static_cast<H const&>(*this),
				make_io_args<get_current_path_t>(buffer)(vsm_forward(args)...));
		}

		template<typename Path>
		[[nodiscard]] vsm::result<Path> get_current_path(auto&&... args)
		{
			vsm::result<Path> r(vsm::result_value);
			vsm_try_discard(get_current_path(any_path_buffer(*r), vsm_forward(args)...));
			return r;
		}
	};

	static vsm::result<void> blocking_io(
		get_current_path_t,
		native_type const& h,
		io_parameters_t<get_current_path_t> const& args);
};

} // namespace allio::detail
