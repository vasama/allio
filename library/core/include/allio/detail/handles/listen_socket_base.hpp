#pragma once

#include <allio/detail/any_endpoint_buffer.hpp>
#include <allio/detail/handle.hpp>
#include <allio/detail/handles/socket_base.hpp>

namespace allio::detail {

struct listen_backlog_t : explicit_argument<listen_backlog_t, uint32_t> {};
inline constexpr explicit_parameter<listen_backlog_t> listen_backlog = {};

struct listen_t
{
	using operation_concept = producer_t;

	struct params_base : io_flags_t
	{
		uint32_t backlog;
		any_endpoint_view endpoint;
	};

	template<typename Object>
	using params_type_template = socket_params<
		params_base,
		typename Object::security_context_type>;

	using result_type = void;
	using runtime_tag = bounded_runtime_t;

	template<object Object>
	static vsm::result<void> blocking_io(
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

	struct params_type
		: io_flags_t
		, deadline_t
	{
		any_endpoint_buffer endpoint;

		using io_flags_t::set_argument;
		using deadline_t::set_argument;

		void set_argument(any_endpoint_buffer const value)
		{
			endpoint = value;
		}
	};

	template<handle Handle>
	using result_type_template =
		typename Handle::template rebind_object<
			typename Handle::object_type::socket_object_type>;

	template<object Object>
	static vsm::result<basic_detached_handle<typename Object::socket_object_type>> blocking_io(
		native_handle<Object> const& h,
		io_parameters_t<Object, accept_t> const& args)
		requires requires { Object::accept(h, args); }
	{
		return Object::accept(h, args);
	}
};

template<object BaseObject>
struct listen_socket_base_t : common_socket_base_t<BaseObject>
{
	using base_type = common_socket_base_t<BaseObject>;

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

template<object BaseObject>
void _listen_socket_object(listen_socket_base_t<BaseObject> const&);

template<typename T>
concept listen_socket_object = requires (T const& t) { _listen_socket_object(t); };

template<listen_socket_object Socket, typename Traits>
[[nodiscard]] auto listen(any_endpoint_view const endpoint, auto&&... args)
{
	auto a = io_parameters_t<Socket, listen_t>{};
	a.endpoint = endpoint;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<Socket, listen_t>(a);
}

} // namespace allio::detail
