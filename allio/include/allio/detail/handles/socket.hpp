#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/deadline.hpp>
#include <allio/detail/handles/platform_object.hpp>
#include <allio/detail/io_sender.hpp>
#include <allio/detail/network_security.hpp>
#include <allio/network.hpp>

#include <vsm/lazy.hpp>
#include <vsm/platform.h>

namespace allio::detail {
namespace socket_io {

struct connect_t
{
	using operation_concept = producer_t;
	using required_params_type = network_endpoint_t;
	using result_type = void;

	template<typename SocketObject>
	using optional_params_template = parameters_t
	<
		basic_socket_security_context_t<typename SocketObject::security_context_type>,
		deadline_t
	>;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, connect_t>,
		typename Object::native_type& h,
		io_parameters_t<Object, connect_t> const& args)
	{
		return Object::connect(h, args);
	}
};

struct disconnect_t
{
	using operation_concept = consumer_t;
	using required_params_type = no_parameters_t;
	using optional_params_type = no_parameters_t;
	using result_type = void;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, disconnect_t>,
		typename Object::native_type& h,
		io_parameters_t<Object, disconnect_t> const& args)
	{
		return Object::disconnect(h, args);
	}
};

struct stream_read_t
{
	using operation_concept = void;
	using required_params_type = read_buffers_t;
	using optional_params_type = deadline_t;
	using result_type = size_t;

	template<object Object>
	friend vsm::result<size_t> tag_invoke(
		blocking_io_t<Object, stream_read_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, stream_read_t> const& args)
	{
		return Object::read(h, args);
	}
};

struct stream_write_t
{
	using operation_concept = void;
	using required_params_type = write_buffers_t;
	using optional_params_type = deadline_t;
	using result_type = size_t;

	template<object Object>
	friend vsm::result<size_t> tag_invoke(
		blocking_io_t<Object, stream_write_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, stream_write_t> const& args)
	{
		return Object::write(h, args);
	}
};

} // namespace socket_io

template<object BaseObject>
struct basic_socket_t : BaseObject
{
	using base_type = BaseObject;

	using connect_t = socket_io::connect_t;
	using disconnect_t = socket_io::disconnect_t;
	using read_some_t = socket_io::stream_read_t;
	using write_some_t = socket_io::stream_write_t;

	using operations = type_list_append
	<
		typename base_type::operations
		, connect_t
		, disconnect_t
		, read_some_t
		, write_some_t
	>;

	template<typename Handle, optional_multiplexer_handle_for<basic_socket_t> MultiplexerHandle>
	struct concrete_interface : base_type::template concrete_interface<Handle, MultiplexerHandle>
	{
		[[nodiscard]] auto read_some(read_buffer const buffer, auto&&... args) const
		{
			return generic_io<read_some_t>(
				static_cast<Handle const&>(*this),
				make_io_args<typename Handle::object_type, read_some_t>(buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto read_some(read_buffers const buffers, auto&&... args) const
		{
			return generic_io<read_some_t>(
				static_cast<Handle const&>(*this),
				make_io_args<typename Handle::object_type, read_some_t>(buffers)(vsm_forward(args)...));
		}

		[[nodiscard]] auto write_some(write_buffer const buffer, auto&&... args) const
		{
			return generic_io<write_some_t>(
				static_cast<Handle const&>(*this),
				make_io_args<typename Handle::object_type, write_some_t>(buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto write_some(write_buffers const buffers, auto&&... args) const
		{
			return generic_io<write_some_t>(
				static_cast<Handle const&>(*this),
				make_io_args<typename Handle::object_type, write_some_t>(buffers)(vsm_forward(args)...));
		}
	};
};

template<object BaseObject>
void _socket_object(basic_socket_t<BaseObject> const&);

template<typename T>
concept socket_object = requires (T const& t) { _socket_object(t); };


struct raw_socket_t : basic_socket_t<platform_object_t>
{
	using security_context_type = void;

	using base_type = basic_socket_t<platform_object_t>;

	struct native_type : base_type::native_type
	{
	};

	static vsm::result<void> connect(
		native_type& h,
		io_parameters_t<raw_socket_t, connect_t> const& args);

	static vsm::result<size_t> read(
		native_type const& h,
		io_parameters_t<raw_socket_t, read_some_t> const& args);

	static vsm::result<size_t> write(
		native_type const& h,
		io_parameters_t<raw_socket_t, write_some_t> const& args);

	static vsm::result<void> close(
		native_type& h,
		io_parameters_t<raw_socket_t, close_t> const& args);
};
using abstract_raw_socket_handle = abstract_handle<raw_socket_t>;


class socket_security_context;

struct socket_t : basic_socket_t<object_t>
{
	using security_context_type = socket_security_context;

	using base_type = basic_socket_t<object_t>;

	struct native_type : base_type::native_type
	{
	};

	static vsm::result<void> connect(
		native_type& h,
		io_parameters_t<socket_t, connect_t> const& args);

	static vsm::result<size_t> read(
		native_type const& h,
		io_parameters_t<socket_t, read_some_t> const& args);

	static vsm::result<size_t> write(
		native_type const& h,
		io_parameters_t<socket_t, write_some_t> const& args);

	static vsm::result<void> close(
		native_type& h,
		io_parameters_t<socket_t, close_t> const& args);
};
using abstract_socket_handle = abstract_handle<socket_t>;

class socket_security_context
{
	//std::unique_ptr<security_> m_security_context;

public:
	//[[nodiscard]] static vsm::result<socket_security_context> create(security_context_parameters const& args);
};


namespace _socket_b {

using socket_handle = blocking_handle<socket_t>;

template<socket_object Object = socket_t>
vsm::result<blocking_handle<Object>> connect(network_endpoint const& endpoint, auto&&... args)
{
	vsm::result<blocking_handle<Object>> r(vsm::result_value);
	vsm_try_void(blocking_io<Object, socket_io::connect_t>(
		*r,
		make_io_args<Object, socket_io::connect_t>(endpoint)(vsm_forward(args)...)));
	return r;
}


using raw_socket_handle = blocking_handle<raw_socket_t>;

vsm::result<raw_socket_handle> raw_connect(network_endpoint const& endpoint, auto&&... args)
{
	return connect<raw_socket_t>(endpoint, vsm_forward(args)...);
}

} // namespace _socket_b

namespace _socket_a {

template<multiplexer_handle_for<socket_t> MultiplexerHandle>
using basic_socket_handle = async_handle<socket_t, MultiplexerHandle>;

template<socket_object Object = socket_t>
ex::sender auto connect(network_endpoint const& endpoint, auto&&... args)
{
	return io_handle_sender<Object, socket_io::connect_t>(
		vsm_lazy(make_io_args<Object, socket_io::connect_t>(endpoint)(vsm_forward(args)...)));
}


template<multiplexer_handle_for<raw_socket_t> MultiplexerHandle>
using basic_raw_socket_handle = async_handle<raw_socket_t, MultiplexerHandle>;

ex::sender auto raw_connect(network_endpoint const& endpoint, auto&&... args)
{
	return connect<raw_socket_t>(endpoint, vsm_forward(args)...);
}

} // namespace _socket_a

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/socket.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/socket.hpp>
#endif
