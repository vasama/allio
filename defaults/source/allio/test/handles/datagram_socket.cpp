#include <allio/blocking/raw_datagram_socket.hpp>
#include <allio/senders/raw_datagram_socket.hpp>

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

TEST_CASE("Datagram socket can send and receive data", "[datagram_socket][blocking]")
{
	using namespace blocking;

	auto const endpoint_factory = test::generate_endpoint_factory();
	if (!is_supported_address_kind(endpoint_factory->address_kind()))
	{
		return;
	}

	auto const server_endpoint = endpoint_factory->create_endpoint();
	auto const server_socket = raw_bind(server_endpoint);

	SECTION("The server socket has no data available to read")
	{
		signed char value = 0;
		try
		{
			(void)server_socket.receive_from(
				as_read_buffer(&value, 1),
				deadline::instant());
		}
		catch (std::system_error const& e)
		{
			REQUIRE(e.code().default_error_condition() == std::errc::timed_out);
		}
	}

	SECTION("The client can send data to the server")
	{
		auto const client_endpoint = endpoint_factory->create_endpoint();
		auto const client_socket = raw_bind(client_endpoint);

		signed char value = 42;
		client_socket.send_to(server_endpoint, as_write_buffer(&value, 1));

		static_cast<volatile signed char&>(value) = 0;
		auto const r = server_socket.receive_from(as_read_buffer(&value, 1));
		REQUIRE(r.size == 1);
		REQUIRE(value == 42);
	}
}

TEST_CASE("Datagram socket can asynchronously send and receive data", "[datagram_socket][async]")
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
