#include <allio/directory_handle.hpp>

using namespace allio;

result<void> detail::directory_handle_base::block_open(filesystem_handle const* const base, input_path_view const path, open_parameters const& args)
{
	return block<io::filesystem_open>(static_cast<directory_handle&>(*this), args, base, path);
}


result<void> detail::directory_stream_handle_base::block_open(filesystem_handle const* const base, input_path_view const path, open_parameters const& args)
{
	return block<io::directory_stream_open_path>(static_cast<directory_stream_handle&>(*this), args, base, path);
}

result<void> detail::directory_stream_handle_base::block_open(directory_handle const& directory, open_parameters const& args)
{
	return block<io::directory_stream_open_handle>(static_cast<directory_stream_handle&>(*this), args, &directory);
}

result<bool> detail::directory_stream_handle_base::block_fetch(fetch_parameters const& args)
{
	return block<io::directory_stream_fetch>(static_cast<directory_stream_handle&>(*this), args);
}

result<size_t> detail::directory_stream_handle_base::block_get_name(entry const& entry, output_string_ref const& output, get_name_parameters const& args) const
{
	return block<io::directory_stream_get_name>(static_cast<directory_stream_handle const&>(*this), args, &entry, output);
}


result<directory_stream_handle> detail::block_open_directory_stream(filesystem_handle const* const base, input_path_view const path, directory_stream_handle::open_parameters const& args)
{
	result<directory_stream_handle> directory_stream_result(result_value);
	allio_TRYV(directory_stream_result->open(base, path, args));
	return directory_stream_result;
}

result<directory_stream_handle> detail::block_open_directory_stream(directory_handle const& directory, directory_stream_handle::open_parameters const& args)
{
	result<directory_stream_handle> directory_stream_result(result_value);
	allio_TRYV(directory_stream_result->open(directory, args));
	return directory_stream_result;
}
