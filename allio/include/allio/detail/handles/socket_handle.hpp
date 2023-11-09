#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/socket_handle_base.hpp>
#include <allio/detail/io_sender.hpp>

#include <vsm/lazy.hpp>
#include <vsm/standard.hpp>

namespace allio::detail {
namespace socket_io {

struct connect_t
{
	using mutation_tag = producer_t;

	using result_type = void;
	using required_params_type = network_endpoint_t;
	using optional_params_type = deadline_t;
};

struct disconnect_t
{
	using mutation_tag = consumer_t;

	using result_type = void;
	using required_params_type = no_parameters_t;
	using optional_params_type = no_parameters_t;
};

struct stream_read_t
{
	using result_type = size_t;
	using required_params_type = untyped_buffers_t;
	using optional_params_type = deadline_t;
};

struct stream_write_t
{
	using result_type = size_t;
	using required_params_type = untyped_buffers_t;
	using optional_params_type = deadline_t;
};

} // namespace socket_io

template<std::derived_from<object_t> BaseHandleTag>
struct basic_socket_handle_t : BaseHandleTag
{
	using base_type = BaseHandleTag;

	using connect_t = socket_io::connect_t;
	using disconnect_t = socket_io::disconnect_t;
	using read_some_t = socket_io::stream_read_t;
	using write_some_t = socket_io::stream_write_t;

	using operations = type_list<
		connect_t,
		disconnect_t,
		read_some_t,
		write_some_t
	>;

	template<typename H, typename M>
	struct concrete_interface : base_type::template concrete_interface<H, M>
	{
		[[nodiscard]] auto read_some(read_buffer const buffer, auto&&... args) const
		{
			return generic_io<read_some_t>(
				static_cast<H const&>(*this),
				io_args<read_some_t>(buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto read_some(read_buffers const buffers, auto&&... args) const
		{
			return generic_io<read_some_t>(
				static_cast<H const&>(*this),
				io_args<read_some_t>(buffers)(vsm_forward(args)...));
		}

		[[nodiscard]] auto write_some(write_buffer const buffer, auto&&... args) const
		{
			return generic_io<write_some_t>(
				static_cast<H const&>(*this),
				io_args<write_some_t>(buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto write_some(write_buffers const buffers, auto&&... args) const
		{
			return generic_io<write_some_t>(
				static_cast<H const&>(*this),
				io_args<write_some_t>(buffers)(vsm_forward(args)...));
		}
	};
};

template<typename BaseHandleTag>
void _socket_handle_tag(basic_socket_handle_t<BaseHandleTag> const&);

template<typename T>
concept socket_handle_tag = requires(T const& tag)
{
	_socket_handle_tag(tag);
};


struct raw_socket_handle_t : basic_socket_handle_t<platform_object_t>
{
	using base_type = basic_socket_handle_t<platform_object_t>;

	using base_type::blocking_io;

	static vsm::result<void> blocking_io(object_t::close_t, native_type& h, io_parameters_t<object_t::close_t> const& args);

	static vsm::result<void> blocking_io(socket_io::connect_t, native_type& h, io_parameters_t<socket_io::connect_t> const& args);
	static vsm::result<void> blocking_io(socket_io::disconnect_t, native_type& h, io_parameters_t<socket_io::disconnect_t> const& args);

	static vsm::result<size_t> blocking_io(socket_io::stream_read_t, native_type const& h, io_parameters_t<socket_io::stream_read_t> const& args);
	static vsm::result<size_t> blocking_io(socket_io::stream_write_t, native_type const& h, io_parameters_t<socket_io::stream_write_t> const& args);
};
using abstract_raw_socket_handle = abstract_handle<raw_socket_handle_t>;

struct socket_handle_t : basic_socket_handle_t<object_t>
{
	using base_type = basic_socket_handle_t<object_t>;

	struct native_type : base_type::native_type
	{
	};

	using base_type::blocking_io;

	static vsm::result<void> blocking_io(socket_io::connect_t, native_type& h, io_parameters_t<socket_io::connect_t> const& args);
	static vsm::result<void> blocking_io(socket_io::disconnect_t, native_type& h, io_parameters_t<socket_io::disconnect_t> const& args);

	static vsm::result<size_t> blocking_io(socket_io::stream_read_t, native_type const& h, io_parameters_t<socket_io::stream_read_t> const& args);
	static vsm::result<size_t> blocking_io(socket_io::stream_write_t, native_type const& h, io_parameters_t<socket_io::stream_write_t> const& args);
};
using abstract_socket_handle = abstract_handle<socket_handle_t>;


namespace _socket_blocking {

using socket_handle = blocking_handle<socket_handle_t>;

template<socket_handle_tag Object = socket_handle_t>
vsm::result<blocking_handle<Object>> connect(network_endpoint const& endpoint, auto&&... args)
{
	vsm::result<blocking_handle<Object>> r(vsm::result_value);
	vsm_try_void(blocking_io<typename Object::connect_t>(
		*r,
		io_args<typename Object::connect_t>(endpoint)(vsm_forward(args)...)));
	return r;
}


using raw_socket_handle = blocking_handle<raw_socket_handle_t>;

vsm::result<raw_socket_handle> raw_connect(network_endpoint const& endpoint, auto&&... args)
{
	return connect<raw_socket_handle_t>(endpoint, vsm_forward(args)...);
}

} // namespace _socket_blocking

namespace _socket_async {

template<typename MultiplexerHandle>
using basic_socket_handle = async_handle<socket_handle_t, MultiplexerHandle>;

template<socket_handle_tag Object = socket_handle_t>
ex::sender auto connect(network_endpoint const& endpoint, auto&&... args)
{
	return io_handle_sender<Object, typename Object::connect_t>(
		vsm_lazy(io_args<typename Object::connect_t>(endpoint)(vsm_forward(args)...)));
}


template<typename MultiplexerHandle>
using basic_raw_socket_handle = async_handle<socket_handle_t, MultiplexerHandle>;

ex::sender auto raw_connect(network_endpoint const& endpoint, auto&&... args)
{
	return connect<raw_socket_handle_t>(endpoint, vsm_forward(args)...);
}

} // namespace _socket_async

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/socket_handle.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/socket_handle.hpp>
#endif
