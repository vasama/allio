#include <allio/blocking/raw_datagram_socket.hpp>
#include <allio/senders/raw_datagram_socket.hpp>

#include <allio/senders/sync_wait.hpp>
#include <allio/senders/task.hpp>
#include <allio/test/match_error.hpp>
#include <allio/test/network.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
namespace ex = stdexec;

static bool is_supported_address_kind(network_address_kind const kind)
{
	//TODO: The async datagram server/client test fails using the local address family, because the
	//      endpoint returned from receive_from is empty in that case. This should be fixed once the
	//      networking operations are changed to take pre-transformed opaque platform addresses.

#if vsm_os_win32 || 1
	if (kind == network_address_kind::local)
	{
		// Windows does not support unix datagram sockets.
		return false;
	}
#endif

	return true;
}

TEST_CASE("Blocking datagram sockets can exchange data", "[datagram_socket][blocking]")
{
	using namespace blocking;

	auto const endpoint_factory = test::generate_endpoint_factory();
	if (!is_supported_address_kind(endpoint_factory->address_kind()))
	{
		return;
	}

	auto const server_endpoint = endpoint_factory->create_endpoint();
	auto const server_socket = raw_bind(server_endpoint);

	signed char value = 0;

	// The server socket has no data available to read:
	REQUIRE_THROWS_MATCHES(
		server_socket.receive_from(as_read_buffer(&value, 1), deadline::instant()),
		std::system_error,
		match_error(std::errc::timed_out));

	auto const client_endpoint = endpoint_factory->create_endpoint();
	auto const client_socket = raw_bind(client_endpoint);

	// Data can be sent to the server through the client socket:
	static_cast<volatile signed char&>(value) = 42;
	client_socket.send_to(server_endpoint, as_write_buffer(&value, 1));

	// Data can be received through the server socket:
	static_cast<volatile signed char&>(value) = 0;
	size_t const size = server_socket.receive_from(as_read_buffer(&value, 1));

	REQUIRE(size == 1);
	REQUIRE(value == 42);

	// The server socket has no data available to read:
	REQUIRE_THROWS_MATCHES(
		server_socket.receive_from(as_read_buffer(&value, 1), deadline::instant()),
		std::system_error,
		match_error(std::errc::timed_out));
}

TEST_CASE("Asynchronous datagram sockets can exchange data", "[datagram_socket][async]")
{
	using namespace senders;

	auto const endpoint_factory = test::generate_endpoint_factory();
	if (!is_supported_address_kind(endpoint_factory->address_kind()))
	{
		return;
	}

	auto multiplexer = default_multiplexer::create().value();

	sync_wait(multiplexer, [&]() -> task<void>
	{
		auto const server_endpoint = endpoint_factory->create_endpoint();
		auto const server_socket = co_await raw_bind(server_endpoint);

		auto const client_endpoint = endpoint_factory->create_endpoint();
		auto const client_socket = co_await raw_bind(client_endpoint);

		co_await ex::when_all
		(
			// Server
			[&]() -> task<void>
			{
				auto& socket = server_socket;
				platform_endpoint peer_endpoint;

				// Wait for a request from the client
				signed char request_data;
				size_t const size = co_await socket.receive_from(
					as_read_buffer(&request_data, 1),
					peer_endpoint);
				REQUIRE(size == 1);

				// Send a reply to the client
				signed char const reply_data = -request_data;
				co_await socket.send_to(peer_endpoint, as_write_buffer(&reply_data, 1));
			}(),

			// Client
			[&]() -> task<void>
			{
				auto& socket = client_socket;

				// Send a request to the server
				signed char const request_data = 42;
				co_await socket.send_to(server_endpoint, as_write_buffer(&request_data, 1));

				// Wait for a reply from the server
				signed char reply_data;
				size_t const size = co_await socket.receive_from(as_read_buffer(&reply_data, 1));
				REQUIRE(size == 1);

				REQUIRE(reply_data == -42);
			}()
		);
	}());
}
