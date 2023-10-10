#pragma once

#include <allio/detail/handles/new_socket_handle_base.hpp>

namespace allio::detail {

template<typename SecurityProvider = default_security_provider>
class abstract_listen_handle : public socket_handle_base<SecurityProvider>
{
protected:
	using base_type = socket_handle_base<SecurityProvider>;

public:
	struct listen_t
	{
		using handle_type = abstract_listen_handle;
		using result_type = void;

		using required_params_type = network_endpoint_t;
		using optional_params_type = no_parameters_t;
	};

	struct accept_t
	{
		using handle_type = abstract_listen_handle;
		using result_type = void;

		using required_params_type = no_parameters_t;
		using optional_params_type = deadline_t;
	};

	using asynchronous_operations = type_list_cat<
		base_type::asynchronous_operations,
		type_list<listen_t, accept_t>
	>;

protected:
	template<typename H>
	struct blocking_interface : base_type::blocking_interface<H>
	{
		vsm::result<abstract_socket_handle> blocking_accept(auto&&... args) const
		{
			vsm::result<abstract_socket_handle> r(vsm::result_value);
			vsm_try_void(do_blocking_io(
				static_cast<H const&>(*this),
				*r,
				io_arguments_t<accept_t>()(vsm_forward(args)...)));
			return r;
		}
	};

	template<typename H>
	struct asynchronous_interface : base_type::asynchronous_interface<H>
	{
		io_sender<H, accept_t> accept(auto&&... args) const
		{
			return io_sender<H, accept_t>(vsm_lazy(io_arguments_t<accept_t>()(vsm_forward(args)...)));
		}
	};

	static vsm::result<void> do_blocking_io(
		abstract_listen_handle& h,
		io_result_t<listen_t>& r,
		io_parameters_t<listen_t> const& args);

	static vsm::result<void> do_blocking_io(
		abstract_listen_handle const& h,
		io_result_t<accept_t>& r,
		io_parameters_t<accept_t> const& args);
};

template<typename Multiplexer = default_multiplexer>
using listen_handle = basic_handle<abstract_listen_handle<>, Multiplexer>;

vsm::result<listen_handle<void>> listen(
	no_multiplexer_t,
	network_endpoint const& endpoint,
	auto&&... args)
{
	using h_type = listen_handle<void>;

	h_type h;
	void_t r;
	vsm_try_void(blocking_io(
		h,
		r,
		io_arguments_t<h_type::listen_t>(endpoint)(vsm_forward(args)...)));
	return h;
}

template<typename Multiplexer>
vsm::result<listen_handle<Multiplexer>> listen(
	Multiplexer&& multiplexer,
	network_endpoint const& endpoint,
	auto&&... args)
{
	using h_type = listen_handle<Multiplexer>;

	h_type h;
	void_t r;
	vsm_try_void(blocking_io(
		h,
		r,
		io_arguments_t<h_type::listen_t>(endpoint)(vsm_forward(args)...)));
	return h;
}

} // namespace allio::detail
