#if 0
#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/completion_port.hpp>
#include <allio/impl/win32/wsa_ex.hpp>
#include <allio/socket_handle.hpp>

#include <catch2/catch_all.hpp>

#include <future>

using namespace allio;
using namespace allio::posix;
using namespace allio::win32;

TEST_CASE("", "[wsa][windows]")
{
	auto const completion_port = create_completion_port(1).value();

	auto const addr = socket_address::make(ipv4_endpoint(ipv4_address::localhost, 51234)).value();
	auto const server_socket = posix::create_socket(network_address_kind::ipv4).value().socket;
	posix::socket_listen(server_socket.get(), addr, nullptr).value();

	auto const client_socket = posix::create_socket(network_address_kind::ipv4).value().socket;
	auto const connect_future = std::async(std::launch::async, [&]()
	{
		posix::socket_connect(client_socket.get(), addr);
	});

	set_completion_information(
		(HANDLE)server_socket.get(),
		completion_port.get(),
		/* key_context: */ nullptr).value();
}
#endif
