#pragma once

#include <allio/detail/handles/new_socket_handle_base.hpp>

namespace allio::detail {

template<typename SecurityProvider = default_security_provider>
class abstract_socket_handle : public socket_handle_base<SecurityProvider>
{
protected:
	using base_type = socket_handle_base<SecurityProvider>;

public:
	struct connect_t
	{
		using handle_type = abstract_socket_handle;
		using result_type = void;

		using required_params_type = network_endpoint_t;
		using optional_params_type = deadline_t;
	};

	struct disconnect_t
	{
		using handle_type = abstract_socket_handle;
		using result_type = void;

		using required_params_type = no_parameters_t;
		using optional_params_type = no_parameters_t;
	};

	struct read_some_t
	{
	};

	struct write_some_t
	{
	};

	using asynchronous_operations = type_list_cat<
		base_type::asynchronous_operations,
		type_list<connect_t, disconnect_t, read_some_t, write_some_t>
	>;

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
		abstract_socket_handle& h,
		io_result_t<connect_t>& r,
		io_parameters_t<connect_t> const& args);

	static vsm::result<void> do_blocking_io(
		abstract_socket_handle& h,
		io_result_t<disconnect_t>& r,
		io_parameters_t<disconnect_t> const& args);

	static vsm::result<void> do_blocking_io(
		abstract_socket_handle const& h,
		io_result_t<read_some_t>& r,
		io_parameters_t<read_some_t> const& args);

	static vsm::result<void> do_blocking_io(
		abstract_socket_handle const& h,
		io_result_t<write_some_t>& r,
		io_parameters_t<write_some_t> const& args);
};

template<typename Multiplexer = default_multiplexer>
using socket_handle = basic_handle<abstract_socket_handle<>, Multiplexer>;

vsm::result<socket_handle<void>> connect(
	no_multiplexer_t,
	network_endpoint const& endpoint,
	auto&&... args)
{
	using h_type = socket_handle<void>;

	h_type h;
	void_t r;
	vsm_try_void(blocking_io(
		h,
		r,
		io_arguments_t<h_type::connect_t>(endpoint)(vsm_forward(args)...)));
	return h;
}

template<typename Multiplexer>
auto connect(
	Multiplexer multiplexer,
	network_endpoint const& endpoint,
	auto&&... args)
{
	using h_type = socket_handle<Multiplexer>;

	return io_handle_sender<h_type::connect_t, h_type>(
		multiplexer,
		io_arguments_t<h_type::connect_t>(endpoint)(vsm_forward(args)...));
}

} // namespace allio::detail
