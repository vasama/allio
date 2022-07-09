#pragma once

#include <allio/detail/api.hpp>
#include <allio/filesystem_handle.hpp>
#include <allio/multiplexer.hpp>

#include <span>

namespace allio {

namespace detail {

class directory_handle_base : public filesystem_handle
{
};

} // namespace detail

using directory_handle = final_handle<detail::directory_handle_base>;
allio_API extern allio_TYPE_ID(directory_handle);

result<directory_handle> open_directory(path_view path);
result<directory_handle> open_directory(filesystem_handle const& base, path_view path);

result<directory_handle> open_unique_directory(filesystem_handle const& base);
result<directory_handle> open_temporary_directory(path_view relative_path = {});

result<directory_handle> open_anonymous_directory();
result<directory_handle> open_anonymous_directory(filesystem_handle const& base);


enum class directory_entry_kind
{
};

template<typename Path>
struct directory_entry
{
	directory_entry_kind kind;
	Path path;
};

namespace io {

template<typename Path>
struct next_directory_entry;

} // namespace io

namespace detail {

class directory_stream_handle_base : public platform_handle
{
public:
	using async_operations = type_list_cat<
		handle::async_operations,
		type_list<
			io::next_directory_entry<basic_path<char>>,
			io::next_directory_entry<basic_path_view<char>>
		>
	>;

	template<typename Char = char>
	result<directory_entry<basic_path<Char>>> next();

	template<typename Char = char>
	result<directory_entry<basic_path_view<Char>>> next(std::span<Char> buffer);
};

} // namespace detail

using directory_stream_handle = final_handle<detail::directory_stream_handle_base>;
allio_API extern allio_TYPE_ID(directory_stream_handle);

result<directory_stream_handle> open_directory_stream(directory_handle&& handle);

template<typename Path>
struct io::parameters<io::next_directory_entry<Path>> : async_operation_parameters
{
	directory_stream_handle const* stream;
	directory_entry<Path>* entry;
};

} // namespace allio
