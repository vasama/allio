#include <allio/datagram_socket.hpp>

#include <allio/sync_wait.hpp>
#include <allio/task.hpp>
#include <allio/test/network.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
namespace ex = stdexec;

static bool is_supported_address_kind(network_address_kind const kind)
{
#if vsm_os_win32
	if (kind == network_address_kind::local)
	{
		// Windows does not support unix datagram sockets.
		return false;
	}
#endif

	return true;
}

TEST_CASE("a datagram socket can send and receive data", "[datagram_socket][blocking]")
{
	using namespace blocking;

	using datagram_socket_object = raw_datagram_socket_t;

	auto const endpoint_factory = test::generate_endpoint_factory();
	if (!is_supported_address_kind(endpoint_factory->address_kind()))
	{
		return;
	}

	auto const server_endpoint = endpoint_factory->create_endpoint();
	auto const server_socket = bind<datagram_socket_object>(server_endpoint).value();

	SECTION("the server socket has no data available to read")
	{
		signed char value = 0;
		auto const r = server_socket.receive_from(as_read_buffer(&value, 1), deadline::instant());
		REQUIRE(r.error() == error::async_operation_timed_out);
	}

	SECTION("the client can send data to the server")
	{
		auto const client_endpoint = endpoint_factory->create_endpoint();
		auto const client_socket = bind<datagram_socket_object>(client_endpoint).value();

		signed char value = 42;
		client_socket.send_to(server_endpoint, as_write_buffer(&value, 1)).value();

		static_cast<volatile signed char&>(value) = 0;
		auto const r = server_socket.receive_from(as_read_buffer(&value, 1)).value();
		REQUIRE(r.size == 1);
		REQUIRE(value == 42);
	}
}

TEST_CASE("a datagram socket can asynchronously send and receive data", "[datagram_socket][async]")
{
	using namespace async;

	using datagram_socket_object = raw_datagram_socket_t;

	auto const endpoint_factory = test::generate_endpoint_factory();
	if (!is_supported_address_kind(endpoint_factory->address_kind()))
	{
		return;
	}

	auto multiplexer = default_multiplexer::create().value();

	sync_wait(multiplexer, [&]() -> task<void>
	{
		auto const server_endpoint = endpoint_factory->create_endpoint();
		auto const server_socket = co_await bind<datagram_socket_object>(server_endpoint);

		auto const client_endpoint = endpoint_factory->create_endpoint();
		auto const client_socket = co_await bind<datagram_socket_object>(client_endpoint);

		co_await ex::when_all
		(
			// Server
			[&]() -> task<void>
			{
				auto& socket = server_socket;

				// Wait for a request from the client
				signed char request_data;
				auto const [size, endpoint] = co_await socket.receive_from(as_read_buffer(&request_data, 1));
				REQUIRE(size == 1);

				// Send a reply to the client
				signed char const reply_data = -request_data;
				co_await socket.send_to(endpoint, as_write_buffer(&reply_data, 1));
			}(),

			// Client
			[&]() -> task<void>
			{
				auto& socket = client_socket;

				// Send a request to the server
				signed char const request_data = 42;
				co_await socket.send_to(server_endpoint, as_write_buffer(&request_data, 1));

				signed char reply_data;
				auto const [size, endpoint] = co_await socket.receive_from(as_read_buffer(&reply_data, 1));
				REQUIRE(size == 1);

				REQUIRE(reply_data == -42);
			}()
		);
	}());
}
