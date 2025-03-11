#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/any_endpoint_buffer.hpp>
#include <allio/detail/handles/common_socket_base.hpp>
#include <allio/detail/deadline.hpp>
#include <allio/detail/handles/socket_params.hpp>
#include <allio/detail/object.hpp>
#include <allio/network.hpp>

namespace allio::detail {

struct bind_t
{
	using operation_concept = producer_t;

	struct params_base : io_flags_t
	{
		any_endpoint_view endpoint;
	};

	template<typename Object>
	using params_type_template = socket_params<
		params_base,
		typename Object::security_context_type>;

	using result_type = void;

	template<object Object>
	static vsm::result<void> blocking_io(
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
		new_read_buffers buffers;
		any_endpoint_buffer endpoint;

		using deadline_t::set_argument;

		void set_argument(any_endpoint_buffer const value)
		{
			endpoint = value;
		}
	};

	using result_type = size_t;

	template<object Object>
	static vsm::result<size_t> blocking_io(
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

	struct params_type : deadline_t
	{
		any_endpoint_view endpoint;
		new_write_buffers buffers;
	};

	using result_type = void;

	template<object Object>
	static vsm::result<void> blocking_io(
		native_handle<Object> const& h,
		io_parameters_t<Object, send_to_t> const& a)
		requires requires { Object::send_to(h, a); }
	{
		return Object::send_to(h, a);
	}
};

template<object BaseObject>
struct datagram_socket_base_t : common_socket_base_t<BaseObject>
{
	using base_type = common_socket_base_t<BaseObject>;

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
		[[nodiscard]] auto receive_from(new_read_buffers const buffers, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, receive_from_t>{};
			a.buffers = buffers;
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<receive_from_t>(static_cast<Handle const&>(*this), a);
		}

		[[nodiscard]] auto send(new_write_buffers const buffers, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, send_to_t>{};
			a.buffers = buffers;
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<send_to_t>(static_cast<Handle const&>(*this), a);
		}

		[[nodiscard]] auto send_to(
			any_endpoint_view const endpoint,
			new_write_buffers const buffers,
			auto&&... args) const
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
void _datagram_socket_object(datagram_socket_base_t<BaseObject> const&);

template<typename T>
concept datagram_socket_object = requires (T const& t)
{
	_datagram_socket_object(t);
};

template<datagram_socket_object Socket, typename Traits>
[[nodiscard]] auto bind(any_endpoint_view const endpoint, auto&&... args)
{
	auto a = io_parameters_t<Socket, bind_t>{};
	a.endpoint = endpoint;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<Socket, bind_t>(a);
}

} // namespace allio::detail
