#pragma once

#include <allio/network.hpp>
#include <allio/platform_handle.hpp>

namespace allio {

namespace io {

struct socket_create;

} // namespace io

class common_socket_handle : public platform_handle
{
public:
	using base_type = platform_handle;

	using async_operations = type_list_cat<
		platform_handle::async_operations,
		type_list<io::socket_create>
	>;


	template<parameters<create_parameters> Parameters = create_parameters::interface>
	vsm::result<void> create(network_address_kind const address_kind, Parameters const& args = {})
	{
		return block_create(address_kind, args);
	}

	template<parameters<common_socket_handle::create_parameters> Parameters = create_parameters::interface>
	basic_sender<io::socket_create> create_async(network_address_kind address_kind, Parameters const& args = {});

protected:
	explicit constexpr common_socket_handle(type_id<common_socket_handle> const type)
		: base_type(type)
	{
	}

	vsm::result<void> block_create(network_address_kind address_kind, create_parameters const& args);

	vsm::result<void> create_sync(network_address_kind address_kind, create_parameters const& args);
};


template<>
struct io::parameters<io::socket_create>
	: common_socket_handle::create_parameters
{
	using handle_type = common_socket_handle;
	using result_type = void;

	network_address_kind address_kind;
};

} // namespace allio
