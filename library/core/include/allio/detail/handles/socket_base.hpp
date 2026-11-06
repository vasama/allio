#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/any_endpoint_buffer.hpp>
#include <allio/detail/handles/common_socket_base.hpp>
#include <allio/detail/deadline.hpp>
#include <allio/detail/handles/socket_params.hpp>
#include <allio/detail/object.hpp>
#include <allio/detail/optional_flags.hpp>
#include <allio/network.hpp>

#include <vsm/platform.h>

namespace allio::detail {

enum class disconnect_mode : uint8_t
{
	read                        = 1 << 1,
	write                       = 1 << 2,
	both                        = read | write,
};
vsm_flag_enum(disconnect_mode);

struct connect_t
{
	using operation_concept = producer_t;

	struct params_base
		: io_flags_t
		, deadline_t
	{
		any_endpoint_view endpoint;

		using io_flags_t::set_argument;
		using deadline_t::set_argument;
	};

	template<typename Object>
	using params_type_template = socket_params<
		params_base,
		typename Object::security_context_type>;

	using result_type = void;

	template<object Object>
	static constexpr auto& blocking_io = Object::connect;
};

struct disconnect_t
{
	using operation_concept = void;

	struct params_type : deadline_t
	{
		optional_flags<disconnect_mode> mode;
	};

	using result_type = void;

	template<object Object>
	static constexpr auto& blocking_io = Object::disconnect;
};

struct get_endpoint_t
{
	using operation_concept = void;

	struct params_type
	{
		any_endpoint_buffer endpoint;
	};

	using result_type = void;

	template<object Object>
	static constexpr auto& blocking_io = Object::get_endpoint;
};

template<object BaseObject>
struct socket_base_t : common_socket_base_t<BaseObject>
{
	using base_type = common_socket_base_t<BaseObject>;

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
	struct vsm_empty_bases facade
		: base_type::template facade<Handle, Traits>
		, byte_io::stream_facade<Handle, Traits>
	{
		[[nodiscard]] auto disconnect(auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, disconnect_t>();
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<disconnect_t>(static_cast<Handle const&>(*this), a);
		}

		[[nodiscard]] auto get_endpoint(any_endpoint_buffer const endpoint, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, get_endpoint_t>();
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<get_endpoint_t>(static_cast<Handle const&>(*this), a);
		}
	};
};

template<object BaseObject>
void _socket_object(socket_base_t<BaseObject> const&);

template<typename T>
concept socket_object = requires (T const& t) { _socket_object(t); };

template<socket_object Socket, typename Traits>
[[nodiscard]] auto connect(any_endpoint_view const endpoint, auto&&... args)
{
	auto a = io_parameters_t<Socket, connect_t>{};
	a.endpoint = endpoint;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<Socket, connect_t>(a);
}

} // namespace allio::detail
