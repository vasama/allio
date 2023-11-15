#include <allio/openssl/socket.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::openssl;

TEST_CASE("OpenSSL sockets can asynchronously connect and exchange data", "[openssl][socket][async]")
{
	using namespace async;

	auto const endpoint = test::generate_endpoint();

	auto multiplexer = default_multiplexer::create().value();

	sync_wait(multiplexer, [&]() -> task<void>
	{
		// Make sure the listening socket is bound before the client attempts to connect.
		auto const listen_socket = co_await listen<openssl::listen_socket_t>(endpoint);

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
				auto const socket = co_await connect<openssl::socket_t>(endpoint);

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
