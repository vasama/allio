#pragma once

#include <allio/async_fwd.hpp>
#include <allio/stream_socket_handle.hpp>
#include <allio/detail/api.hpp>

#include <cstdint>

namespace allio {

struct accept_result
{
	stream_socket_handle socket;
	network_address address;
};

namespace io {

struct socket_listen;
struct socket_accept;

} // namespace io

namespace detail {

class listen_socket_handle_base : public common_socket_handle
{
	using final_handle_type = final_handle<listen_socket_handle_base>;

public:
	using base_type = common_socket_handle;

	using async_operations = type_list_cat<
		common_socket_handle::async_operations,
		type_list<
			io::socket_listen,
			io::socket_accept
		>
	>;


	#define allio_listen_socket_handle_listen_parameters(type, data, ...) \
		type(allio::detail::listen_socket_handle_base, listen_parameters) \
		allio_platform_handle_create_parameters(__VA_ARGS__, __VA_ARGS__) \
		data(uint32_t, backlog, 0) \

	allio_interface_parameters(allio_listen_socket_handle_listen_parameters);

	using accept_parameters = common_socket_handle::create_parameters;


	constexpr listen_socket_handle_base()
		: base_type(type_of<final_handle_type>())
	{
	}


	template<parameters<listen_parameters> Parameters = listen_parameters::interface>
	vsm::result<void> listen(network_address const& address, Parameters const& args = {})
	{
		return block_listen(address, args);
	}

	template<parameters<listen_socket_handle_base::listen_parameters> Parameters = listen_parameters::interface>
	basic_sender<io::socket_listen> listen_async(network_address const& address, Parameters const& args = {});


	template<parameters<accept_parameters> Parameters = accept_parameters::interface>
	vsm::result<accept_result> accept(Parameters const& args = {}) const
	{
		return block_accept(args);
	}

	template<parameters<listen_socket_handle_base::accept_parameters> Parameters = accept_parameters::interface>
	basic_sender<io::socket_accept> accept_async(Parameters const& args = {}) const;

private:
	vsm::result<void> block_listen(network_address const& address, listen_parameters const& args);
	vsm::result<accept_result> block_accept(accept_parameters const& args) const;

protected:
	using base_type::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::socket_create> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::socket_listen> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::socket_accept> const& args);

	template<typename H, typename O>
	friend vsm::result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};

vsm::result<final_handle<listen_socket_handle_base>> block_listen(
    network_address const& address,
    listen_socket_handle_base::listen_parameters const& args);

} // namespace detail

using listen_socket_handle = final_handle<detail::listen_socket_handle_base>;

template<parameters<listen_socket_handle::listen_parameters> Parameters = listen_socket_handle::listen_parameters::interface>
vsm::result<listen_socket_handle> listen(network_address const& address, Parameters const& args = {})
{
	return detail::block_listen(address, args);
}


template<>
struct io::parameters<io::socket_listen>
	: listen_socket_handle::listen_parameters
{
	using handle_type = listen_socket_handle;
	using result_type = void;

	network_address address;
};

template<>
struct io::parameters<io::socket_accept>
	: listen_socket_handle::accept_parameters
{
	using handle_type = listen_socket_handle const;
	using result_type = accept_result;
};

allio_detail_api extern allio_handle_implementation(listen_socket_handle);

} // namespace allio
