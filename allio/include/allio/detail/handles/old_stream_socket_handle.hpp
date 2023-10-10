#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/common_socket_handle.hpp>
#include <allio/detail/io_sender.hpp>

namespace allio::detail {

class _listen_socket_handle;

class _stream_socket_handle : public common_socket_handle
{
protected:
	using base_type = common_socket_handle;

public:
	struct connect_t
	{
		using handle_type = _stream_socket_handle;
		using result_type = void;
		struct required_params_type
		{
			network_endpoint endpoint;
		};
		using optional_params_type = deadline_t;
	};

	using async_operations = type_list_cat
	<
		base_type::async_operations,
		type_list
		<
			connect_t,
			scatter_read_t,
			gather_write_t
		>
	>;

protected:
	allio_detail_default_lifetime(_stream_socket_handle);

	template<typename H>
	struct sync_interface : base_type::sync_interface<H>
	{
		vsm::result<size_t> read_some(read_buffers const buffers, auto&&... args) const
		{
			vsm::result<size_t> r(vsm::result_value);
			vsm_try_void(do_blocking_io(
				static_cast<H const&>(*this),
				*r,
				io_arguments_t<scatter_read_t>(buffers)(vsm_forward(args)...)));
			return r;
		}

		vsm::result<size_t> read_some(read_buffer const buffer, auto&&... args) const
		{
			vsm::result<size_t> r(vsm::result_value);
			vsm_try_void(do_blocking_io(
				static_cast<H const&>(*this),
				*r,
				io_arguments_t<scatter_read_t>(buffer)(vsm_forward(args)...)));
			return r;
		}

		vsm::result<size_t> write_some(write_buffers const buffers, auto&&... args) const
		{
			vsm::result<size_t> r(vsm::result_value);
			vsm_try_void(do_blocking_io(
				static_cast<H const&>(*this),
				*r,
				io_arguments_t<gather_write_t>(buffers)(vsm_forward(args)...)));
			return r;
		}

		vsm::result<size_t> write_some(write_buffer const buffer, auto&&... args) const
		{
			vsm::result<size_t> r(vsm::result_value);
			vsm_try_void(do_blocking_io(
				static_cast<H const&>(*this),
				*r,
				io_arguments_t<gather_write_t>(buffer)(vsm_forward(args)...)));
			return r;
		}
	};
	
	template<typename H>
	struct async_interface : base_type::async_interface<H>
	{
		io_sender<H, scatter_read_t> read_async(read_buffers const buffers, auto&&... args) const;
		io_sender<H, scatter_read_t> read_async(read_buffer const buffer, auto&&... args) const;
		io_sender<H, gather_write_t> write_async(write_buffers const buffers, auto&&... args) const;
		io_sender<H, gather_write_t> write_async(write_buffer const buffer, auto&&... args) const;
	};

	static vsm::result<void> do_blocking_io(
		_stream_socket_handle& h,
		io_result_ref_t<connect_t> result,
		io_parameters_t<connect_t> const& args);

	static vsm::result<void> do_blocking_io(
		_stream_socket_handle const& h,
		io_result_ref_t<scatter_read_t> result,
		io_parameters_t<scatter_read_t> const& args);

	static vsm::result<void> do_blocking_io(
		_stream_socket_handle const& h,
		io_result_ref_t<gather_write_t> result,
		io_parameters_t<gather_write_t> const& args);

	friend _listen_socket_handle;
};

using blocking_stream_socket_handle = basic_blocking_handle<_stream_socket_handle>;

template<typename Multiplexer>
using basic_stream_socket_handle = basic_async_handle<_stream_socket_handle, Multiplexer>;


vsm::result<blocking_stream_socket_handle> connect(network_endpoint const& endpoint, auto&&... args)
{
	vsm::result<blocking_stream_socket_handle> r(vsm::result_value);
	vsm_try_void(blocking_io(
		*r,
		no_result,
		io_arguments_t<_stream_socket_handle::connect_t>(endpoint)(vsm_forward(args)...)));
	return r;
}

} // namespace allio::detail
