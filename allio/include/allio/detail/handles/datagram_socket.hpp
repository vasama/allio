#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/deadline.hpp>
#include <allio/detail/handles/platform_object.hpp>
#include <allio/detail/io_sender.hpp>
#include <allio/network.hpp>

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
	using operation_concept = producer_t;

	struct params_type : io_flags_t
	{
		network_endpoint endpoint;
	};

	using result_type = void;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, bind_t>,
		typename Object::native_type& h,
		io_parameters_t<Object, bind_t> const& a)
		requires requires { Object::bind(h, a); }
	{
		return Object::bind(h, a);
	}
};

struct receive_from_t
{
	using operation_concept = void;

	struct params_type : deadline_t
	{
		read_buffers_storage buffers;
	};

	using result_type = receive_result;

	template<object Object>
	friend vsm::result<receive_result> tag_invoke(
		blocking_io_t<Object, receive_from_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, receive_from_t> const& a)
		requires requires { Object::receive_from(h, a); }
	{
		return Object::receive_from(h, a);
	}
};

struct send_to_t
{
	using operation_concept = void;
	
	struct params_type
	{
		network_endpoint endpoint;
		write_buffers_storage buffers;
	};

	using result_type = void;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, send_to_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, send_to_t> const& a)
		requires requires { Object::send_to(h, a); }
	{
		return Object::send_to(h, a);
	}
};

} // namespace socket_io

template<object BaseObject>
struct basic_datagram_socket_t : BaseObject
{
	using base_type = BaseObject;

	using bind_t = socket_io::bind_t;
	using receive_from_t = socket_io::receive_from_t;
	using send_to_t = socket_io::send_to_t;

	using operations = type_list_append
	<
		typename base_type::operations
		, bind_t
		, send_to_t
		, receive_from_t
	>;

	template<typename Handle>
	struct concrete_interface : base_type::template concrete_interface<Handle>
	{
		[[nodiscard]] auto receive_from(read_buffer const buffer, auto&&... args) const
		{
			io_parameters_t<typename Handle::object_type, receive_from_t> a = {};
			a.buffers = buffer;
			(set_argument(a, vsm_forward(args)), ...);
			return Handle::io_traits_type::template observe<receive_from_t>(
				static_cast<Handle const&>(*this),
				a);
		}

		[[nodiscard]] auto receive_from(read_buffers const buffers, auto&&... args) const
		{
			io_parameters_t<typename Handle::object_type, receive_from_t> a = {};
			a.buffers = buffers;
			(set_argument(a, vsm_forward(args)), ...);
			return Handle::io_traits_type::template observe<receive_from_t>(
				static_cast<Handle const&>(*this),
				a);
		}

		[[nodiscard]] auto send(write_buffer const buffer, auto&&... args) const
		{
			io_parameters_t<typename Handle::object_type, send_to_t> a = {};
			a.buffers = buffer;
			(set_argument(a, vsm_forward(args)), ...);
			return Handle::io_traits_type::template observe<send_to_t>(
				static_cast<Handle const&>(*this),
				a);
		}

		[[nodiscard]] auto send(write_buffers const buffers, auto&&... args) const
		{
			io_parameters_t<typename Handle::object_type, send_to_t> a = {};
			a.buffers = buffers;
			(set_argument(a, vsm_forward(args)), ...);
			return Handle::io_traits_type::template observe<send_to_t>(
				static_cast<Handle const&>(*this),
				a);
		}

		[[nodiscard]] auto send_to(network_endpoint const& endpoint, write_buffer const buffer, auto&&... args) const
		{
			io_parameters_t<typename Handle::object_type, send_to_t> a = {};
			a.endpoint = endpoint;
			a.buffers = buffer;
			(set_argument(a, vsm_forward(args)), ...);
			return Handle::io_traits_type::template observe<send_to_t>(
				static_cast<Handle const&>(*this),
				a);
		}

		[[nodiscard]] auto send_to(network_endpoint const& endpoint, write_buffers const buffers, auto&&... args) const
		{
			io_parameters_t<typename Handle::object_type, send_to_t> a = {};
			a.endpoint = endpoint;
			a.buffers = buffers;
			(set_argument(a, vsm_forward(args)), ...);
			return Handle::io_traits_type::template observe<send_to_t>(
				static_cast<Handle const&>(*this),
				a);
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

	struct native_type : base_type::native_type
	{
	};

	static vsm::result<void> bind(
		native_type& h,
		io_parameters_t<raw_datagram_socket_t, bind_t> const& args);

	static vsm::result<receive_result> receive_from(
		native_type const& h,
		io_parameters_t<raw_datagram_socket_t, receive_from_t> const& args);

	static vsm::result<void> send_to(
		native_type const& h,
		io_parameters_t<raw_datagram_socket_t, send_to_t> const& args);

	static vsm::result<void> close(
		native_type& h,
		io_parameters_t<raw_datagram_socket_t, close_t> const& args);
};

struct datagram_socket_t : basic_datagram_socket_t<object_t>
{
	using base_type = basic_datagram_socket_t<object_t>;

	struct native_type : base_type::native_type
	{
	};
};

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/datagram_socket.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/datagram_socket.hpp>
#endif
