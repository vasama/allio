#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/deadline.hpp>
#include <allio/detail/object.hpp>
#include <allio/network.hpp>

namespace allio::detail {

struct connect_t
{
	using operation_concept = producer_t;

	struct params_type : io_flags_t, deadline_t
	{
		network_endpoint endpoint;
	};

	using result_type = void;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<connect_t>,
		native_handle<Object>& h,
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
		blocking_io_t<disconnect_t>,
		native_handle<Object>& h,
		io_parameters_t<Object, disconnect_t> const& args)
		requires requires { Object::disconnect(h, args); }
	{
		return Object::disconnect(h, args);
	}
};

template<object BaseObject>
struct basic_socket_t : BaseObject
{
	using base_type = BaseObject;

	using connect_t = detail::connect_t;
	using disconnect_t = detail::disconnect_t;
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

	template<typename Handle, typename Traits>
	struct facade
		: base_type::template facade<Handle, Traits>
		//TODO: This introduces padding on MSVC.
		, byte_io::stream_facade<Handle, Traits>
	{
	};
};

template<object BaseObject>
void _socket_object(basic_socket_t<BaseObject> const&);

template<typename T>
concept socket_object = requires (T const& t) { _socket_object(t); };

template<socket_object Socket, typename Traits>
[[nodiscard]] auto connect(network_endpoint const& endpoint, auto&&... args)
{
	auto a = io_parameters_t<Socket, connect_t>{};
	a.endpoint = endpoint;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<Socket, connect_t>(a);
}

} // namespace allio::detail
