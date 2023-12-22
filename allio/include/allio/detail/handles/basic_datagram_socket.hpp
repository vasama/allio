#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/deadline.hpp>
#include <allio/detail/object.hpp>
#include <allio/network.hpp>

namespace allio::detail {

struct receive_result
{
	size_t size;
	network_endpoint endpoint;
};

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
		blocking_io_t<bind_t>,
		native_handle<Object>& h,
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
		blocking_io_t<receive_from_t>,
		native_handle<Object> const& h,
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
		blocking_io_t<send_to_t>,
		native_handle<Object> const& h,
		io_parameters_t<Object, send_to_t> const& a)
		requires requires { Object::send_to(h, a); }
	{
		return Object::send_to(h, a);
	}
};

template<object BaseObject>
struct basic_datagram_socket_t : BaseObject
{
	using base_type = BaseObject;

	using bind_t = detail::bind_t;
	using receive_from_t = detail::receive_from_t;
	using send_to_t = detail::send_to_t;

	using operations = type_list_append
	<
		typename base_type::operations
		, bind_t
		, send_to_t
		, receive_from_t
	>;

	template<typename Handle, typename Traits>
	struct facade : base_type::template facade<Handle, Traits>
	{
		[[nodiscard]] auto receive_from(read_buffer const buffer, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, receive_from_t>{};
			a.buffers = buffer;
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<receive_from_t>(static_cast<Handle const&>(*this), a);
		}

		[[nodiscard]] auto receive_from(read_buffers const buffers, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, receive_from_t>{};
			a.buffers = buffers;
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<receive_from_t>(static_cast<Handle const&>(*this), a);
		}

		[[nodiscard]] auto send(write_buffer const buffer, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, send_to_t>{};
			a.buffers = buffer;
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<send_to_t>(static_cast<Handle const&>(*this), a);
		}

		[[nodiscard]] auto send(write_buffers const buffers, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, send_to_t>{};
			a.buffers = buffers;
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<send_to_t>(static_cast<Handle const&>(*this), a);
		}

		[[nodiscard]] auto send_to(network_endpoint const& endpoint, write_buffer const buffer, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, send_to_t>{};
			a.endpoint = endpoint;
			a.buffers = buffer;
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<send_to_t>(static_cast<Handle const&>(*this), a);
		}

		[[nodiscard]] auto send_to(network_endpoint const& endpoint, write_buffers const buffers, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, send_to_t>{};
			a.endpoint = endpoint;
			a.buffers = buffers;
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<send_to_t>(static_cast<Handle const&>(*this), a);
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

template<datagram_socket_object Socket, typename Traits>
[[nodiscard]] auto bind(network_endpoint const& endpoint, auto&&... args)
{
	auto a = io_parameters_t<Socket, bind_t>{};
	a.endpoint = endpoint;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<Socket, bind_t>(a);
}

} // namespace allio::detail
