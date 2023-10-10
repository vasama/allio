#pragma once

#include <allio/detail/handles/new_socket_handle_base.hpp>

namespace allio::detail {

struct datagram_read_result
{
	size_t size;
	network_endpoint endpoint;
};

template<typename SecurityProvider = default_security_provider>
class abstract_datagram_handle : public socket_handle_base<SecurityProvider>
{
protected:
	using base_type = socket_handle_base<SecurityProvider>;

public:
	struct bind_t
	{
		using handle_type = abstract_datagram_handle;
		using result_type = void;

		using required_params_type = network_endpoint_t;
		using optional_params_type = no_parameters_t;
	};

	struct send_t
	{
		using handle_type = abstract_datagram_handle const;
		using result_type = void;

		struct required_params_type
		{
			network_endpoint endpoint;
			untyped_buffers_storage buffers;
		};

		using optional_params_type = no_parameters_t;
	};

	struct receive_t
	{
		using handle_type = abstract_datagram_handle const;
		using result_type = datagram_read_result;

		struct required_params_type
		{
			untyped_buffers_storage buffers;
		};

		using optional_params_type = no_parameters_t;
	};


	using asynchronous_operations = type_list_cat<
		base_type::asynchronous_operations,
		type_list<bind_t, send_t, receive_t>>;

protected:
	template<typename H>
	struct blocking_interface : base_type::blocking_interface<H>
	{
	};

	template<typename H>
	struct asynchronous_interface : base_type::asynchronous_interface<H>
	{
	};

	static vsm::result<void> do_blocking_io(
		abstract_listen_handle& h,
		io_result_t<listen_t>& r,
		io_parameters_t<listen_t> const& args);
};

template<typename Multiplexer = default_multiplexer>
using datagram_handle = basic_handle<abstract_datagram_handle<>, Multiplexer>;

vsm::result<datagram_handle<void>> bind(
	no_multiplexer_t,
	network_endpoint const& endpoint
	auto&&... args)
{
	using h_type = datagram_handle<void>;

	h_type h;
	void_t r;
	vsm_try_void(blocking_io(
		h,
		r,
		io_arguments_t<h_type::bind_t>(endpoint)(vsm_forward(args)...)));
	return h;
}

template<typename Multiplexer>
vsm::result<datagram_handle<Multiplexer>> listen(
	Multiplexer&& multiplexer,
	network_endpoint const& endpoint,
	auto&&... args)
{
	using h_type = datagram_handle<Multiplexer>;

	h_type h;
	void_t r;
	vsm_try_void(blocking_io(
		h,
		r,
		io_arguments_t<h_type::bind_t>(endpoint)(vsm_forward(args)...)));
	return h;
}

} // namespace allio::detail
