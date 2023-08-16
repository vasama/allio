#pragma once

#include <allio/detail/handles/stream_socket_handle.hpp>

namespace allio {
namespace detail {

struct accept_result
{
	stream_socket_handle socket;
	network_address address;
};

class _listen_socket_handle : public common_socket_handle
{
protected:
	using base_type = common_socket_handle;

public:
	struct listen_t;
	#define allio_listen_socket_handle_listen_parameters(type, data, ...) \
		type(allio::detail::listen_socket_handle_base, listen_parameters) \
		allio_platform_handle_create_parameters(__VA_ARGS__, __VA_ARGS__) \
		data(uint32_t, backlog, 0) \

	allio_interface_parameters(allio_listen_socket_handle_listen_parameters);


	struct accept_t;
	using accept_parameters = common_socket_handle::create_parameters;

protected:
	allio_detail_default_lifetime(_listen_socket_handle);

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
		basic_sender<listen_t> listen_async(network_address const& address, P const& args = {}) &;
		
		template<parameters<accept_parameters> P = accept_parameters::interface>
		basic_sender<accept_t> accept_async(P const& args = {}) const;
	};
};

} // namespace detail

template<>
struct io_operation_traits<listen_socket_handle::listen_t>
{
	using handle_type = listen_socket_handle;
	using result_type = void;
	using params_type = listen_socket_handle::listen_parameters;

	network_address address;
};

template<>
struct io_operation_traits<listen_socket_handle::listen_t>
{
	using handle_type = listen_socket_handle;
	using result_type = accept_result;
	using params_type = listen_socket_handle::accept_parameters;
};

} // namespace allio
