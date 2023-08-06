#pragma once

#include <allio/filesystem.hpp>
#include <allio/input_path_view.hpp>
#include <allio/multiplexer.hpp>
#include <allio/output_path_ref.hpp>
#include <allio/platform_handle.hpp>
#include <allio/path.hpp>
#include <allio/path_view.hpp>
#include <allio/type_list.hpp>

#include <filesystem>
#include <span>

namespace allio {

class filesystem_handle;

namespace io {

struct filesystem_open;
struct get_current_path;

} // namespace io


enum class file_mode : uint8_t
{
	unchanged,
	none,
	read_attributes,
	write_attributes,
	read,
	write,
	append,

#if 0
	none                                = 0b0010,
	attr_read                           = 0b0100,
	attr_write                          = 0b0101,
	read                                = 0b0110,
	write                               = 0b0111,
	append                              = 0b1001,
#endif
};

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
	unchanged                                               = 0,
	none                                                    = 1,
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
#	define allio_detail_win32_file_flag(x) 1 << x
#else
#	define allio_detail_win32_file_flag(x) 0
#endif

	win_disable_unlink_emulation                            = allio_detail_win32_file_flag(24),
	win_disable_sparse_file_creation                        = allio_detail_win32_file_flag(25),

#undef allio_detail_win32_file_flag
};

enum class path_kind : uint32_t
{
	any                                                     = static_cast<uint32_t>(-1),

	win_nt                                                  = 1 << 0,
	win_guid                                                = 1 << 1,
	win_dos                                                 = 1 << 2,
};
vsm_flag_enum(path_kind);

struct input_path
{
	filesystem_handle const* base;
	input_path_view path;
};


class filesystem_handle : public platform_handle
{
public:
	using base_type = platform_handle;

	using async_operations = type_list_cat<
		base_type::async_operations,
		type_list<
			io::filesystem_open,
			io::get_current_path
		>
	>;

	#define allio_filesystem_handle_open_parameters(type, data, ...) \
		type(allio::filesystem_handle, open_parameters) \
		allio_platform_handle_create_parameters(__VA_ARGS__, __VA_ARGS__) \
		data(::allio::file_mode,            mode,           ::allio::file_mode::read) \
		data(::allio::file_creation,        creation,       ::allio::file_creation::open_existing) \
		data(::allio::file_creation,        creation,       ::allio::file_sharing::all) \
		data(::allio::file_caching,         caching,        ::allio::file_caching::none) \
		data(::allio::file_flags,           flags,          ::allio::file_flags::none) \

	allio_interface_parameters(allio_filesystem_handle_open_parameters);

	#define allio_filesystem_handle_get_current_path_parameters(type, data, ...) \
		type(allio::filesystem_handle, get_current_path_parameters) \
		data(::allio::path_kind,            kind,           ::allio::path_kind::any) \

	allio_interface_parameters(allio_filesystem_handle_get_current_path_parameters);


	template<typename Path = path, parameters<get_current_path_parameters> P = get_current_path_parameters::interface>
	vsm::result<Path> get_current_path(output_path_ref const output, P const& args = {}) const
	{
		Path path = {};
		vsm_try_discard(block_get_current_path(output, args));
		return static_cast<Path&&>(path);
	}

	template<parameters<get_current_path_parameters> P = get_current_path_parameters::interface>
	vsm::result<size_t> get_current_path(output_path_ref const output, P const& args = {}) const
	{
		return block_get_current_path(output, args);
	}

protected:
	explicit constexpr filesystem_handle(type_id<filesystem_handle> const type)
		: base_type(type)
	{
	}

	filesystem_handle(filesystem_handle&&) = default;
	filesystem_handle& operator=(filesystem_handle&&) = default;
	~filesystem_handle() = default;


	vsm::result<size_t> block_get_current_path(output_path_ref output, get_current_path_parameters const& args) const;


	using base_type::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::get_current_path> const& args);

	template<typename H, typename O>
	friend vsm::result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};


vsm::result<file_attributes> get_file_attributes(input_path_view path);
vsm::result<file_attributes> get_file_attributes(filesystem_handle const& base, input_path_view path);


template<>
struct io::parameters<io::filesystem_open>
	: filesystem_handle::open_parameters
{
	using handle_type = filesystem_handle;
	using result_type = void;

	filesystem_handle const* base;
	input_path_view path;
};

template<>
struct io::parameters<io::get_current_path>
	: filesystem_handle::get_current_path_parameters
{
	using handle_type = filesystem_handle const;
	using result_type = size_t;

	output_path_ref output;
};

} // namespace allio
