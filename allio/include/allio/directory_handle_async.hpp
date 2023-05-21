#pragma once

#include <allio/directory_handle.hpp>

#include <allio/async.hpp>

namespace allio {
namespace detail {

template<parameters<directory_stream_handle_base::fetch_parameters> P>
basic_sender<io::directory_stream_fetch> directory_stream_handle_base::fetch_async(P const& args)
{
	return { static_cast<directory_stream_handle&>(*this), args };
}


inline auto open_directory_stream_async_impl(multiplexer& multiplexer,
	filesystem_handle const* const base, input_path_view const path,
	directory_stream_handle::open_parameters const& args)
{
	return initialize_handle<directory_stream_handle>(multiplexer, [=](auto& h)
	{
	});
}

inline auto open_directory_stream_async_impl(multiplexer& multiplexer,
	directory_handle const* const directory,
	directory_stream_handle_base::open_parameters const& args)
{
	return initialize_handle<directory_stream_handle>(multiplexer, [=](auto& h)
	{
	});
}

} // namespace detail

template<parameters<directory_stream_handle::open_parameters> P = directory_stream_handle::open_parameters::interface>
auto open_directory_stream_async(multiplexer& multiplexer, input_path_view const path, P const& args = {})
{
	return detail::open_directory_stream_async_impl(multiplexer, nullptr, path, args);
}

template<parameters<directory_stream_handle::open_parameters> P = directory_stream_handle::open_parameters::interface>
auto open_directory_stream_async(multiplexer& multiplexer, filesystem_handle const& base, input_path_view const path, P const& args = {})
{
	return detail::open_directory_stream_async_impl(multiplexer, &base, path, args);
}

template<parameters<directory_stream_handle::open_parameters> P = directory_stream_handle::open_parameters::interface>
auto open_directory_stream_async(multiplexer& multiplexer, directory_handle const& directory, P const& args = {})
{
	return detail::open_directory_stream_async_impl(multiplexer, &directory, args);
}

} // namespace allio
