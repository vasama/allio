#include <allio/listen_socket.hpp>
#include <allio/socket.hpp>

#include <allio/sync_wait.hpp>
#include <allio/task.hpp>
#include <allio/test/network.hpp>

#include <catch2/catch_all.hpp>

#include <future>

using namespace allio;
namespace ex = stdexec;

TEST_CASE("a stream socket can connect to a listening socket and exchange data", "[socket_handle][blocking]")
{
	using namespace blocking;

	using socket_object = raw_socket_t;
	using listen_socket_object = raw_listen_socket_t;

	auto const endpoint = test::generate_endpoint();
	auto const listen_socket = listen<listen_socket_object>(endpoint).value();

	auto connect_future = std::async(std::launch::async, [&]()
	{
		return connect<socket_object>(endpoint);
	});

	auto const server_socket = listen_socket.accept().value().socket;
	auto const client_socket = connect_future.get().value();

	SECTION("the server socket has no data to read")
	{
		signed char value = 0;
		auto const r = server_socket.read_some(as_read_buffer(&value, 1), deadline::instant());
		REQUIRE(r.error() == error::async_operation_timed_out);
	}

	SECTION("the client socket has no data to read")
	{
		signed char value = 0;
		auto const r = client_socket.read_some(as_read_buffer(&value, 1), deadline::instant());
		REQUIRE(r.error() == error::async_operation_timed_out);
	}

	SECTION("the client can send data to the server")
	{
		signed char value = 42;
		REQUIRE(client_socket.write_some(as_write_buffer(&value, 1)).value() == 1);

		static_cast<volatile signed char&>(value) = 0;
		REQUIRE(server_socket.read_some(as_read_buffer(&value, 1)).value() == 1);
		REQUIRE(value == 42);
	}

	SECTION("the server can send data to the client")
	{
		signed char value = 42;
		REQUIRE(server_socket.write_some(as_write_buffer(&value, 1)).value() == 1);

		static_cast<volatile signed char&>(value) = 0;
		REQUIRE(client_socket.read_some(as_read_buffer(&value, 1)).value() == 1);
		REQUIRE(value == 42);
	}
}

TEST_CASE("a stream socket can asynchronously connect to a listening socket and exchange data", "[socket_handle][async]")
{
	using namespace async;

	auto const endpoint = test::generate_endpoint();

	auto multiplexer = default_multiplexer::create().value();

	sync_wait(multiplexer, [&]() -> task<void>
	{
		// Make sure the listening socket is bound before the client attempts to connect.
		auto const listen_socket = co_await raw_listen(endpoint);

		co_await ex::when_all
		(
			// Server
			[&]() -> task<void>
			{
				// Accept client connection
				auto const& [socket, _] = co_await listen_socket.accept();

				// Wait for a request from the client
				signed char request_data;
				REQUIRE(co_await socket.read_some(as_read_buffer(&request_data, 1)) == 1);

				// Send a reply to the client
				signed char const reply_data = -request_data;
				REQUIRE(co_await socket.write_some(as_write_buffer(&reply_data, 1)) == 1);
			}(),

			// Client
			[&]() -> task<void>
			{
				// Connect to the server
				auto const socket = co_await raw_connect(endpoint);

				// Send a request to the server
				signed char const request_data = 42;
				REQUIRE(co_await socket.write_some(as_write_buffer(&request_data, 1)) == 1);

				// Wait for a reply from the server
				signed char reply_data;
				REQUIRE(co_await socket.read_some(as_read_buffer(&reply_data, 1)) == 1);

				REQUIRE(reply_data == -42);
			}()
		);
	}());
}
