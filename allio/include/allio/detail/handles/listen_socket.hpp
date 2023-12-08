#pragma once

#include <allio/any_path_buffer.hpp>
#include <allio/detail/handles/socket.hpp>
#include <allio/detail/io_sender.hpp>

#include <vsm/lazy.hpp>

namespace allio::detail {

struct listen_backlog_t
{
	std::optional<uint32_t> backlog;
};
inline constexpr explicit_parameter<listen_backlog_t, uint32_t> listen_backlog = {};

template<typename SocketHandle>
struct accept_result
{
	SocketHandle socket;
	network_endpoint endpoint;

	template<typename ToSocketHandle>
	friend vsm::result<accept_result<ToSocketHandle>> tag_invoke(
		rebind_handle_t<accept_result<ToSocketHandle>>,
		accept_result&& self,
		auto&&... args)
	{
		vsm_try(new_socket, rebind_handle<ToSocketHandle>(vsm_move(self.socket), vsm_forward(args)...));
		return vsm_lazy(accept_result<ToSocketHandle>{ vsm_move(new_socket), self.endpoint });
	}
};

namespace socket_io {

struct listen_t
{
	using operation_concept = producer_t;
	using required_params_type = network_endpoint_t;
	using optional_params_type = parameters_t
	<
		inheritable_t,
		listen_backlog_t
	>;

	template<object Object>
	using extended_params_template = with_security_context_t<typename Object::security_context_type>;

	using result_type = void;
	using runtime_tag = bounded_runtime_t;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, listen_t>,
		typename Object::native_type& h,
		io_parameters_t<Object, listen_t> const& args)
		requires requires { Object::listen(h, args); }
	{
		return Object::listen(h, args);
	}
};

struct accept_t
{
	using operation_concept = void;
	using required_params_type = no_parameters_t;
	using optional_params_type = parameters_t
	<
		inheritable_t,
		deadline_t
	>;

	template<object Object, optional_multiplexer_handle_for<Object> MultiplexerHandle>
	using result_type_template = accept_result<basic_handle<typename Object::socket_object_type, MultiplexerHandle>>;

	template<object Object>
	friend vsm::result<accept_result<blocking_handle<raw_socket_t>>> tag_invoke(
		blocking_io_t<Object, accept_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, accept_t> const& args)
		requires requires { Object::accept(h, args); }
	{
		return Object::accept(h, args);
	}
};

} // namespace socket_io

template<object BaseObject, object SocketObject>
struct basic_listen_socket_t : BaseObject
{
	using socket_object_type = SocketObject;

	using base_type = BaseObject;

	using listen_t = socket_io::listen_t;
	using accept_t = socket_io::accept_t;

	using operations = type_list_append
	<
		typename base_type::operations
		, listen_t
		, accept_t
	>;

	template<typename Handle, optional_multiplexer_handle_for<basic_listen_socket_t> MultiplexerHandle>
	struct concrete_interface : base_type::template concrete_interface<Handle, MultiplexerHandle>
	{
		using socket_handle_type = basic_handle<SocketObject, MultiplexerHandle>;
		using accept_result_type = accept_result<socket_handle_type>;

		[[nodiscard]] auto accept(auto&&... args) const
		{
			return generic_io<accept_t>(
				static_cast<Handle const&>(*this),
				make_io_args<typename Handle::object_type, accept_t>()(vsm_forward(args)...));
		}
	};
};

template<object BaseObject, object SocketObject>
void _listen_socket_object(basic_listen_socket_t<BaseObject, SocketObject> const&);

template<typename T>
concept listen_socket_object = requires (T const& t) { _listen_socket_object(t); };


struct raw_listen_socket_t : basic_listen_socket_t<platform_object_t, raw_socket_t>
{
	using security_context_type = void;

	using base_type = basic_listen_socket_t<platform_object_t, raw_socket_t>;

	struct native_type : base_type::native_type
	{
	};

	static vsm::result<void> listen(
		native_type& h,
		io_parameters_t<raw_listen_socket_t, listen_t> const& a);

	static vsm::result<accept_result<blocking_handle<raw_socket_t>>> accept(
		native_type const& h,
		io_parameters_t<raw_listen_socket_t, accept_t> const& a);

	static vsm::result<void> close(
		native_type& h,
		io_parameters_t<raw_listen_socket_t, close_t> const& a);
};
using abstract_raw_listen_handle = abstract_handle<raw_listen_socket_t>;


class listen_socket_security_context
{
public:
};

struct listen_socket_t : basic_listen_socket_t<object_t, socket_t>
{
	using security_context_type = listen_socket_security_context;

	using base_type = basic_listen_socket_t<object_t, socket_t>;

	struct native_type : base_type::native_type
	{
	};

	static vsm::result<void> listen(
		socket_io::listen_t,
		native_type& h,
		io_parameters_t<listen_socket_t, socket_io::listen_t> const& a);

	static vsm::result<accept_result<blocking_handle<socket_t>>> accept(
		native_type const& h,
		io_parameters_t<listen_socket_t, socket_io::accept_t> const& a);
};
using abstract_listen_handle = abstract_handle<listen_socket_t>;


namespace _listen_b {

using listen_handle = blocking_handle<listen_socket_t>;

template<listen_socket_object Object = listen_socket_t>
vsm::result<blocking_handle<Object>> listen(network_endpoint const& endpoint, auto&&... args)
{
	vsm::result<blocking_handle<Object>> r(vsm::result_value);
	vsm_try_void(blocking_io<Object, socket_io::listen_t>(
		*r,
		make_io_args<Object, socket_io::listen_t>(endpoint)(vsm_forward(args)...)));
	return r;
}


using raw_listen_handle = blocking_handle<raw_listen_socket_t>;

vsm::result<raw_listen_handle> raw_listen(network_endpoint const& endpoint, auto&&... args)
{
	return listne<raw_listen_socket_t>(endpoint, vsm_forward(args)...);
}

} // namespace _listen_b

namespace _listen_a {

template<multiplexer_handle_for<listen_socket_t> MultiplexerHandle>
using basic_listen_handle = async_handle<listen_socket_t, MultiplexerHandle>;

template<listen_socket_object Object = listen_socket_t>
ex::sender auto listen(network_endpoint const& endpoint, auto&&... args)
{
	return io_handle_sender<Object, socket_io::listen_t>(
		vsm_lazy(make_io_args<Object, socket_io::listen_t>(endpoint)(vsm_forward(args)...)));
}


template<multiplexer_handle_for<raw_listen_socket_t> MultiplexerHandle>
using basic_raw_listen_handle = async_handle<raw_listen_socket_t, MultiplexerHandle>;

ex::sender auto raw_listen(network_endpoint const& endpoint, auto&&... args)
{
	return listen<raw_listen_socket_t>(endpoint, vsm_forward(args)...);
}

} // namespace _listen_a

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/listen_socket.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/listen_socket.hpp>
#endif
