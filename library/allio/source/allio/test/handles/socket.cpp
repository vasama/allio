#include <allio/blocking/raw_listen_socket.hpp>
#include <allio/blocking/raw_socket.hpp>
#include <allio/senders/raw_listen_socket.hpp>
#include <allio/senders/raw_socket.hpp>

#include <allio/handles/object.hpp>
#include <allio/senders/sync_wait.hpp>
#include <allio/senders/task.hpp>
#include <allio/test/match_error.hpp>
#include <allio/test/network.hpp>
#include <allio/test/spawn.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
namespace ex = stdexec;

TEST_CASE("Blocking stream sockets can exchange data", "[socket][blocking]")
{
	using namespace blocking;

	auto const endpoint = test::generate_endpoint();
	auto const listen_socket = raw_listen(endpoint);

	// Connect in a background thread.
	auto connect_future = test::spawn([&]()
	{
		return raw_connect(endpoint);
	});

	auto const server_socket = listen_socket.accept();
	auto client_socket = connect_future.get();

	// The server socket has no data to read:
	{
		signed char value = 0;
		REQUIRE_THROWS_MATCHES(
			(void)server_socket.read_some(as_read_buffer(&value, 1), deadline::instant()),
			std::system_error,
			match_error(std::errc::timed_out));
	}

	// The client socket has no data to read:
	{
		signed char value = 0;
		REQUIRE_THROWS_MATCHES(
			(void)client_socket.read_some(as_read_buffer(&value, 1), deadline::instant()),
			std::system_error,
			match_error(std::errc::timed_out));
	}

	// The client can send data to the server:
	{
		signed char value = 42;
		REQUIRE(client_socket.write_some(as_write_buffer(&value, 1)) == 1);
	}

	// The server can receive and send data from and to the client:
	{
		signed char value = 0;
		REQUIRE(server_socket.read_some(as_read_buffer(&value, 1)) == 1);

		value = -value;
		REQUIRE(server_socket.write_some(as_write_buffer(&value, 1)) == 1);
	}

	// The client can receive data from the server:
	{
		signed char value = 0;
		REQUIRE(client_socket.read_some(as_read_buffer(&value, 1)) == 1);
		REQUIRE(value == -42);

		client_socket.close();
	}

	// The server can no longer read after client close:
	{
		signed char value = 0;
		REQUIRE_THROWS_MATCHES(
			server_socket.read_some(as_read_buffer(&value, 1)),
			std::system_error,
			match_error(error::end_of_stream));
	}
}

TEST_CASE("Asynchronous stream sockets can exchange data", "[socket][async]")
{
	using namespace senders;

	auto multiplexer = default_multiplexer::create().value();

	sync_wait(multiplexer, []() -> task<void>
	{
		auto const endpoint = test::generate_endpoint();

		// Make sure the listening socket is bound before the client attempts to connect.
		auto const listen_socket = co_await raw_listen(endpoint);

		co_await ex::when_all
		(
			// Server
			[&]() -> task<void>
			{
				platform_endpoint peer_endpoint;

				// Accept client connection:
				auto const socket = co_await listen_socket.accept(peer_endpoint);

#if 0 //TODO: IOCP implementation doesn't support timeouts yet.
				// The socket has no data to read:
				signed char unused_data;
				REQUIRE_THROWS_MATCHES(
					(void)co_await socket.read_some(
						as_read_buffer(&unused_data, 1),
						deadline::instant()),
					std::system_error,
					match_error(std::errc::timed_out));
#endif

				// Wait for a request from the client:
				signed char request_data;
				REQUIRE(co_await socket.read_some(as_read_buffer(&request_data, 1)) == 1);

				// Send a reply to the client:
				signed char const reply_data = -request_data;
				REQUIRE(co_await socket.write_some(as_write_buffer(&reply_data, 1)) == 1);

				// Any further read should result in an error:
				REQUIRE_THROWS_MATCHES(
					co_await socket.read_some(as_read_buffer(&request_data, 1)),
					std::system_error,
					match_error(error::end_of_stream));
			}(),

			// Client
			[&]() -> task<void>
			{
				// Connect to the server:
				auto const socket = co_await raw_connect(endpoint);

#if 0
				// The socket has no data to read:
				signed char unused_data;
				REQUIRE_THROWS_MATCHES(
					(void)co_await socket.read_some(
						as_read_buffer(&unused_data, 1),
						deadline::instant()),
					std::system_error,
					match_error(std::errc::timed_out));
#endif

				// Send a request to the server:
				signed char const request_data = 42;
				REQUIRE(co_await socket.write_some(as_write_buffer(&request_data, 1)) == 1);

				// Wait for a reply from the server:
				signed char reply_data;
				REQUIRE(co_await socket.read_some(as_read_buffer(&reply_data, 1)) == 1);

				REQUIRE(reply_data == -42);
			}()
		);
	}());
}
