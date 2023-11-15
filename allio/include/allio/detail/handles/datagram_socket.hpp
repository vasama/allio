#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/socket_handle_base.hpp>
#include <allio/detail/io_sender.hpp>

#include <vsm/lazy.hpp>
#include <vsm/platform.h>

namespace allio::detail {

struct receive_result
{
	size_t size;
	network_endpoint endpoint;
};

namespace socket_io {

struct bind_t
{
	using mutation_tag = producer_t;

	using result_type = void;
	using required_params_type = network_endpoint_t;
	using optional_params_type = no_parameters_t;
};

struct send_to_t
{
	using result_type = void;
	using required_params_type = parameters_t<network_endpoint_t, untyped_buffers_t>;
	using optional_params_type = deadline_t;
};

struct receive_from_t
{
	using result_type = receive_result;
	using required_params_type = untyped_buffers_t;
	using optional_params_type = deadline_t;
};

} // namespace socket_io

template<std::derived_from<object_t> BaseObject>
struct basic_datagram_socket_t : BaseObject
{
	using base_type = BaseObject;

	using bind_t = socket_io::bind_t;
	using send_to_t = socket_io::send_to_t;
	using receive_from_t = socket_io::receive_from_t;

	using operations = type_list_cat
	<
		typename base_type::operations,
		type_list
		<
			bind_t,
			send_to_t,
			receive_from_t
		>
	>;
	
	template<typename H, typename M>
	struct concrete_interface : base_type::template concrete_interface<H, M>
	{
		[[nodiscard]] auto send(write_buffer const buffer, auto&&... args) const
		{
			return generic_io<send_to_t>(
				static_cast<H const&>(*this),
				io_args<send_to_t>(null_endpoint, buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto send(write_buffers const buffers, auto&&... args) const
		{
			return generic_io<send_to_t>(
				static_cast<H const&>(*this),
				io_args<send_to_t>(null_endpoint, buffers)(vsm_forward(args)...));
		}

		[[nodiscard]] auto send_to(network_endpoint const& endpoint, write_buffer const buffer, auto&&... args) const
		{
			return generic_io<send_to_t>(
				static_cast<H const&>(*this),
				io_args<send_to_t>(endpoint, buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto send_to(network_endpoint const& endpoint, write_buffers const buffers, auto&&... args) const
		{
			return generic_io<send_to_t>(
				static_cast<H const&>(*this),
				io_args<send_to_t>(endpoint, buffers)(vsm_forward(args)...));
		}

		[[nodiscard]] auto receive_from(read_buffer const buffer, auto&&... args) const
		{
			return generic_io<receive_from_t>(
				static_cast<H const&>(*this),
				io_args<receive_from_t>(buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto receive_from(read_buffers const buffers, auto&&... args) const
		{
			return generic_io<receive_from_t>(
				static_cast<H const&>(*this),
				io_args<receive_from_t>(buffers)(vsm_forward(args)...));
		}
	};
};

template<typename BaseObject>
void _datagram_socket_object(basic_datagram_socket_t<BaseObject> const&);

template<typename T>
concept datagram_socket_object = requires (T const& t)
{
	_datagram_socket_object(t);
};


struct raw_datagram_socket_t : basic_datagram_socket_t<platform_object_t>
{
	using base_type = basic_datagram_socket_t<platform_object_t>;

	using base_type::blocking_io;

	static vsm::result<void> blocking_io(
		close_t,
		native_type& h,
		io_parameters_t<close_t> const& args);

	static vsm::result<void> blocking_io(
		bind_t,
		native_type& h,
		io_parameters_t<bind_t> const& args);

	static vsm::result<void> blocking_io(
		send_to_t,
		native_type const& h,
		io_parameters_t<send_to_t> const& args);

	static vsm::result<receive_result> blocking_io(
		receive_from_t,
		native_type const& h,
		io_parameters_t<receive_from_t> const& args);
};
using abstract_raw_datagram_socket_handle = abstract_handle<raw_datagram_socket_t>;

struct datagram_socket_t : basic_datagram_socket_t<object_t>
{
	using base_type = basic_datagram_socket_t<object_t>;

	struct native_type : base_type::native_type
	{
	};
};
using abstract_datagram_socket_handle = abstract_handle<datagram_socket_t>;


namespace _datagram_socket_b {

using datagram_socket_handle = blocking_handle<datagram_socket_t>;

template<datagram_socket_object Object = datagram_socket_t>
vsm::result<blocking_handle<Object>> bind(network_endpoint const& endpoint, auto&&... args)
{
	vsm::result<blocking_handle<Object>> r(vsm::result_value);
	vsm_try_void(blocking_io<typename Object::bind_t>(
		*r,
		io_args<typename Object::bind_t>(endpoint)(vsm_forward(args)...)));
	return r;
}


using raw_datagram_socket_handle = blocking_handle<raw_datagram_socket_t>;

vsm::result<raw_datagram_socket_handle> raw_bind(network_endpoint const& endpoint, auto&&... args)
{
	return bind<raw_datagram_socket_t>(endpoint, vsm_forward(args)...);
}

} // namespace _datagram_socket_b

namespace _datagram_socket_a {

template<typename MultiplexerHandle>
using basic_datagram_socket_handle = async_handle<datagram_socket_t, MultiplexerHandle>;

template<datagram_socket_object Object = datagram_socket_t>
ex::sender auto bind(network_endpoint const& endpoint, auto&&... args)
{
	return io_handle_sender<Object, typename Object::bind_t>(
		vsm_lazy(io_args<typename Object::bind_t>(endpoint)(vsm_forward(args)...)));
}


template<typename MultiplexerHandle>
using basic_raw_datagram_socket_handle = async_handle<raw_datagram_socket_t, MultiplexerHandle>;

ex::sender auto raw_bind(network_endpoint const& endpoint, auto&&... args)
{
	return bind<raw_datagram_socket_t>(endpoint, vsm_forward(args)...);
}

} // namespace _datagram_socket_a

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/datagram_socket.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/datagram_socket.hpp>
#endif
