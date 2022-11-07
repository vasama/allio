#pragma once

#include <allio/input_path_view.hpp>
#include <allio/multiplexer.hpp>
#include <allio/platform_handle.hpp>
#include <allio/path.hpp>
#include <allio/path_view.hpp>
#include <allio/type_list.hpp>

#include <span>

namespace allio {

class filesystem_handle;

namespace io {

struct filesystem_open;

template<typename Char>
struct get_current_path;

template<typename Char>
struct copy_current_path;

} // namespace io


enum class file_kind
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

class file_id
{
	std::byte m_data alignas(uintptr_t)[16];

public:
	auto operator<=>(file_id const&) const = default;
};

class file_permissions
{
};


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

enum class file_caching : uint8_t
{
	unchanged                           = 0,
	none                                = 1,
};

enum class file_flags : uint32_t
{
	none                                = 0,
	unlink_on_first_close               = 1 << 0,
	disable_safety_barriers             = 1 << 1,
	disable_safety_unlinks              = 1 << 2,
	disable_prefetching                 = 1 << 3,
	maximum_prefetching                 = 1 << 4,

#if allio_detail_WIN32
#	define allio_detail_WIN_FILE_FLAG(x) 1 << x
#else
#	define allio_detail_WIN_FILE_FLAG(x) 0
#endif

	win_disable_unlink_emulation        = allio_detail_WIN_FILE_FLAG(24),
	win_disable_sparse_file_creation    = allio_detail_WIN_FILE_FLAG(25),

#undef allio_detail_WIN_FILE_FLAG
};

enum class path_kind : uint32_t
{
	any                         = static_cast<uint32_t>(-1),

	win_nt                      = 1 << 0,
	win_guid                    = 1 << 1,
	win_dos                     = 1 << 2,
};
allio_detail_FLAG_ENUM(path_kind);


class filesystem_handle : public platform_handle
{
public:
	using base_type = platform_handle;

	using async_operations = type_list_cat<
		base_type::async_operations,
		type_list<
			io::filesystem_open,
			io::get_current_path<char>,
			io::copy_current_path<char>
		>
	>;

	#define allio_FILESYSTEM_HANDLE_OPEN_PARAMETERS(type, data, ...) \
		type(allio::filesystem_handle, open_parameters) \
		allio_PLATFORM_HANDLE_CREATE_PARAMETERS(__VA_ARGS__, __VA_ARGS__) \
		data(::allio::file_mode,            mode,           ::allio::file_mode::read) \
		data(::allio::file_creation,        creation,       ::allio::file_creation::open_existing) \
		data(::allio::file_caching,         caching,        ::allio::file_caching::none) \
		data(::allio::file_flags,           flags,          ::allio::file_flags::none) \

	allio_INTERFACE_PARAMETERS(allio_FILESYSTEM_HANDLE_OPEN_PARAMETERS);

	#define allio_FILESYSTEM_HANDLE_CURRENT_PATH_PARAMETERS(type, data, ...) \
		type(allio::filesystem_handle, current_path_parameters) \
		data(::allio::path_kind,            kind,           ::allio::path_kind::any) \

	allio_INTERFACE_PARAMETERS(allio_FILESYSTEM_HANDLE_CURRENT_PATH_PARAMETERS);


	template<typename Char = char, parameters<current_path_parameters> Parameters = current_path_parameters::interface>
	result<basic_path<Char>> get_current_path(Parameters const& args = {}) const
	{
		return block_get_current_path<Char>(args);
	}

	template<typename Char = char, parameters<current_path_parameters> Parameters = current_path_parameters::interface>
	result<basic_path_view<Char>> copy_current_path(std::span<Char> const buffer, Parameters const& args = {}) const
	{
		return block_copy_current_path<Char>(buffer, args);
	}

protected:
	explicit constexpr filesystem_handle(type_id<filesystem_handle> const type)
		: base_type(type)
	{
	}

	filesystem_handle(filesystem_handle&&) = default;
	filesystem_handle& operator=(filesystem_handle&&) = default;
	~filesystem_handle() = default;


	template<typename Char>
	result<basic_path<Char>> block_get_current_path(current_path_parameters const& args);

	template<typename Char>
	result<basic_path_view<Char>> block_copy_current_path(std::span<Char> buffer, current_path_parameters const& args);


	using base_type::sync_impl;

	template<typename Char>
	static result<void> sync_impl(io::parameters_with_result<io::get_current_path<Char>> const& args);

	template<typename Char>
	static result<void> sync_impl(io::parameters_with_result<io::copy_current_path<Char>> const& args);

	template<typename H, typename O>
	friend result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};


template<>
struct io::parameters<io::filesystem_open>
	: filesystem_handle::open_parameters
{
	using handle_type = filesystem_handle;
	using result_type = void;

	filesystem_handle const* base;
	input_path_view path;
};

template<typename Char>
struct io::parameters<io::get_current_path<Char>>
	: basic_parameters
{
	using handle_type = filesystem_handle;
	using result_type = void;

	path_kind kind;
	basic_path<Char>* path;
};

template<typename Char>
struct io::parameters<io::copy_current_path<Char>>
	: basic_parameters
{
	using handle_type = filesystem_handle;
	using result_type = void;

	path_kind kind;
	std::span<Char> buffer;
};

} // namespace allio
