#pragma once

#include <allio/detail/handles/basic_socket.hpp>

namespace allio::detail {

struct listen_backlog_t : explicit_argument<listen_backlog_t, uint32_t> {};
inline constexpr explicit_parameter<listen_backlog_t> listen_backlog = {};

template<typename SocketHandle>
struct accept_result
{
	SocketHandle socket;
	network_endpoint endpoint;

	template<typename OtherSocketHandle>
	friend vsm::result<accept_result<OtherSocketHandle>> tag_invoke(
		rebind_handle_t<accept_result<OtherSocketHandle>>,
		accept_result&& self,
		auto&&... args)
	{
		using result_type = vsm::result<accept_result<OtherSocketHandle>>;

		auto r = rebind_handle<OtherSocketHandle>(
			vsm_move(self.socket),
			vsm_forward(args)...);

		return r
			? result_type(vsm::result_value, vsm_move(*r), self.endpoint)
			: result_type(vsm::result_error, r.error());
	}
};

struct listen_t
{
	using operation_concept = producer_t;

	struct params_type : io_flags_t
	{
		uint32_t backlog;
		network_endpoint endpoint;
	};

	using result_type = void;
	using runtime_tag = bounded_runtime_t;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<listen_t>,
		native_handle<Object>& h,
		io_parameters_t<Object, listen_t> const& args)
		requires requires { Object::listen(h, args); }
	{
		return Object::listen(h, args);
	}
};

struct accept_t
{
	using operation_concept = void;

	struct params_type : io_flags_t, deadline_t
	{
	};

	//template<object Object, optional_multiplexer_handle MultiplexerHandle>
	//using result_type_template = accept_result<basic_handle<typename Object::socket_object_type, MultiplexerHandle>>;

	template<handle Handle>
	using result_type_template = accept_result<
		typename Handle::template rebind_object<
			typename Handle::object_type::socket_object_type>>;

	template<object Object>
	friend vsm::result<accept_result<basic_detached_handle<typename Object::socket_object_type>>> tag_invoke(
		blocking_io_t<accept_t>,
		native_handle<Object> const& h,
		io_parameters_t<Object, accept_t> const& args)
		requires requires { Object::accept(h, args); }
	{
		return Object::accept(h, args);
	}
};

template<object BaseObject, object SocketObject>
struct basic_listen_socket_t : BaseObject
{
	using socket_object_type = SocketObject;

	using base_type = BaseObject;

	using listen_t = detail::listen_t;
	using accept_t = detail::accept_t;

	using operations = type_list_append
	<
		typename base_type::operations
		, listen_t
		, accept_t
	>;

	template<typename Handle, typename Traits>
	struct facade : base_type::template facade<Handle, Traits>
	{
		[[nodiscard]] auto accept(auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, accept_t>{};
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<accept_t>(static_cast<Handle const&>(*this), a);
		}
	};
};

template<object BaseObject, object SocketObject>
void _listen_socket_object(basic_listen_socket_t<BaseObject, SocketObject> const&);

template<typename T>
concept listen_socket_object = requires (T const& t) { _listen_socket_object(t); };

template<listen_socket_object Socket, typename Traits>
[[nodiscard]] auto listen(network_endpoint const& endpoint, auto&&... args)
{
	auto a = io_parameters_t<Socket, listen_t>{};
	a.endpoint = endpoint;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<Socket, listen_t>(a);
}

} // namespace allio::detail
