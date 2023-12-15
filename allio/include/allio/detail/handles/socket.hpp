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

template<bool IsVoid>
struct _with_security_context;

template<>
struct _with_security_context<0>
{
	template<typename SecurityContext>
	using type = basic_security_context_t<SecurityContext>;
};

template<>
struct _with_security_context<1>
{
	template<typename SecurityContext>
	using type = no_parameters_t;
};

template<typename SecurityContext>
using with_security_context_t = typename _with_security_context<std::is_void_v<SecurityContext>>::template type<SecurityContext>;

namespace socket_io {

struct connect_t
{
	using operation_concept = producer_t;

	struct params_type : io_flags_t, deadline_t
	{
		network_endpoint endpoint;
	};

	template<object Object>
	using extended_params_template = with_security_context_t<typename Object::security_context_type>;

	using result_type = void;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, connect_t>,
		typename Object::native_type& h,
		io_parameters_t<Object, connect_t> const& args)
		requires requires { Object::connect(h, args); }
	{
		return Object::connect(h, args);
	}
};

struct disconnect_t
{
	using operation_concept = consumer_t;
	using params_type = no_parameters_t;
	using result_type = void;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, disconnect_t>,
		typename Object::native_type& h,
		io_parameters_t<Object, disconnect_t> const& args)
		requires requires { Object::disconnect(h, args); }
	{
		return Object::disconnect(h, args);
	}
};

} // namespace socket_io

template<object BaseObject>
struct basic_socket_t : BaseObject
{
	using base_type = BaseObject;

	using connect_t = socket_io::connect_t;
	using disconnect_t = socket_io::disconnect_t;
	using stream_read_t = byte_io::stream_read_t;
	using stream_write_t = byte_io::stream_write_t;

	using operations = type_list_append
	<
		typename base_type::operations
		, connect_t
		, disconnect_t
		, stream_read_t
		, stream_write_t
	>;

	template<typename Handle>
	struct concrete_interface
		: base_type::template concrete_interface<Handle>
		, byte_io::stream_interface<Handle>
	{
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

	static vsm::result<void> disconnect(
		native_type& h,
		io_parameters_t<raw_socket_t, disconnect_t> const& args);

	static vsm::result<size_t> stream_read(
		native_type const& h,
		io_parameters_t<raw_socket_t, stream_read_t> const& args);

	static vsm::result<size_t> stream_write(
		native_type const& h,
		io_parameters_t<raw_socket_t, stream_write_t> const& args);

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
		io_parameters_t<socket_t, stream_read_t> const& args);

	static vsm::result<size_t> write(
		native_type const& h,
		io_parameters_t<socket_t, stream_write_t> const& args);

	static vsm::result<void> close(
		native_type& h,
		io_parameters_t<socket_t, close_t> const& args);
};

class socket_security_context
{
	//std::unique_ptr<security_> m_security_context;

public:
	//[[nodiscard]] static vsm::result<socket_security_context> create(security_context_parameters const& args);
};

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/socket.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/socket.hpp>
#endif
