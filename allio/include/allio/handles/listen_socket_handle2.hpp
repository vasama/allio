#pragma once

#include <allio/handles/stream_socket_handle2.hpp>

namespace allio {

struct accept_result
{
	stream_socket_handle socket;
	network_address address;
};

namespace detail {

class listen_socket_handle_impl : public common_socket_handle
{
protected:
	using base_type = common_socket_handle;

public:
	struct listen_tag;
	#define allio_listen_socket_handle_listen_parameters(type, data, ...) \
		type(allio::detail::listen_socket_handle_base, listen_parameters) \
		allio_platform_handle_create_parameters(__VA_ARGS__, __VA_ARGS__) \
		data(uint32_t, backlog, 0) \

	allio_interface_parameters(allio_listen_socket_handle_listen_parameters);


	struct accept_tag;
	using accept_parameters = common_socket_handle::create_parameters;

protected:
	allio_detail_default_lifetime(listen_socket_handle_impl);

	template<typename H>
	struct sync_interface : base_type::sync_interface<H>
	{
		template<parameters<listen_parameters> P = listen_parameters::interface>
		vsm::result<void> listen(network_address const& address, P const& args = {}) &;
		
		template<parameters<accept_parameters> P = accept_parameters::interface>
		vsm::result<accept_result> accept(P const& args = {}) const;
	};

	template<typename H>
	struct async_interface : base_type::async_interface<H>
	{
		template<parameters<listen_parameters> P = listen_parameters::interface>
		basic_sender<listen_tag> listen_async(network_address const& address, P const& args = {}) &;
		
		template<parameters<accept_parameters> P = accept_parameters::interface>
		basic_sender<accept_tag> accept_async(P const& args = {}) const;
	};
};

} // namespace detail

using listen_socket_handle = basic_handle<detail::listen_socket_handle_impl>;

template<typename Multiplexer>
using basic_listen_socket_handle = basic_async_handle<detail::listen_socket_handle_impl, Multiplexer>;


template<parameters<listen_parameters> P = listen_parameters::interface>
vsm::result<listen_socket_handle> listen(network_address const& address, P const& args = {});

template<typename Multiplexer, parameters<listen_parameters> P = listen_parameters::interface>
auto listen_async(Multiplexer& multiplexer, network_address const& address, P const& args = {});


template<>
struct io_operation_traits<listen_socket_handle::listen_tag>
{
	using handle_type = listen_socket_handle;
	using result_type = void;
	using params_type = listen_socket_handle::listen_parameters;

	network_address address;
};

template<>
struct io_operation_traits<listen_socket_handle::listen_tag>
{
	using handle_type = listen_socket_handle;
	using result_type = accept_result;
	using params_type = listen_socket_handle::accept_parameters;
};

} // namespace allio
