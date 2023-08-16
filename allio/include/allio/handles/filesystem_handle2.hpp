#pragma once

#include <allio/filesystem.hpp>
#include <allio/handles/platform_handle2.hpp>
#include <allio/input_path_view.hpp>
#include <allio/output_path_ref.hpp>
#include <allio/path.hpp>
#include <allio/path_view.hpp>

namespace allio {

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
	input_path_view path;
};

class filesystem_handle : public platform_handle
{
protected:
	using base_type = platform_handle;

public:
	struct open_tag;

	#define allio_filesystem_handle_open_parameters(type, data, ...) \
		type(allio::filesystem_handle, open_parameters) \
		allio_platform_handle_create_parameters(__VA_ARGS__, __VA_ARGS__) \
		data(::allio::file_mode,            mode,           ::allio::file_mode::read_write) \
		data(::allio::file_creation,        creation,       ::allio::file_creation::open_existing) \
		data(::allio::file_sharing,         sharing,        ::allio::file_sharing::all) \
		data(::allio::file_caching,         caching,        ::allio::file_caching::none) \
		data(::allio::file_flags,           flags,          ::allio::file_flags::none) \

	allio_interface_parameters(allio_filesystem_handle_open_parameters);


	struct get_current_path_tag;

	#define allio_filesystem_handle_get_current_path_parameters(type, data, ...) \
		type(allio::filesystem_handle, get_current_path_parameters) \
		data(::allio::path_kind,            kind,           ::allio::path_kind::any) \

	allio_interface_parameters(allio_filesystem_handle_get_current_path_parameters);


	template<parameters<get_current_path_parameters> P = get_current_path_parameters::interface>
	vsm::result<size_t> get_current_path(output_path_ref const output, P const& args = {}) const;

	template<typename Path = path, parameters<get_current_path_parameters> P = get_current_path_parameters::interface>
	vsm::result<Path> get_current_path(output_path_ref const output, P const& args = {}) const;


	using async_operations = type_list_cat
	<
		base_type::async_operations,
		type_list
		<
			open_tag
		>
	>;

protected:
	allio_detail_default_lifetime(filesystem_handle);

	using platform_handle::platform_handle;

	template<typename H>
	struct sync_interface : base_type::sync_interface<H>
	{
		template<parameters<open_parameters> P = open_parameters::interface>
		vsm::result<void> open(path_descriptor const path, P const& args) &;
	};

	template<typename H>
	struct async_interface : base_type::async_interface<H>
	{
		template<parameters<open_parameters> P = open_parameters::interface>
		basic_sender<open_tag> open_async(path_descriptor const path, P const& args) &;
	};
};


vsm::result<file_attributes> query_file_attributes(path_descriptor path);


template<>
struct io_operation_traits<filesystem_handle::open_tag>
{
	using handle_type = filesystem_handle;
	using result_type = void;
	using params_type = filesystem_handle::open_parameters;

	path_descriptor path;
};

template<>
struct io_operation_traits<filesystem_handle, filesystem_handle::get_current_path_tag>
{
	using handle_type = filesystem_handle const;
	using result_type = size_t;
	using params_type = filesystem_handle::get_current_path_parameters;

	output_path_ref output;
};

} // namespace allio
