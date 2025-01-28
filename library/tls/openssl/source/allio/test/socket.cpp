#include <allio/openssl/listen_socket.hpp>
#include <allio/openssl/socket.hpp>

#include <allio/blocking/traits.hpp>
#include <allio/senders.hpp>
#include <allio/senders/sync_wait.hpp>
#include <allio/senders/task.hpp>
#include <allio/test/network.hpp>
#include <allio/test/spawn.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::openssl;

namespace ex = stdexec;

void f(
	detail::native_handle<detail::openssl_socket_t>& h,
	detail::connect_t::params_type_template<detail::openssl_socket_t> const& a)
{
	detail::openssl_socket_t::blocking_io<detail::connect_t>(h, a);
}

TEST_CASE(
	"a stream socket can connect to a listening socket and exchange data",
	"[openssl][socket][blocking]")
{
	using namespace blocking;

	using socket_object = openssl::socket_t;
	using listen_socket_object = openssl::listen_socket_t;

	using namespace path_literals;
	auto const server_security = openssl::create_listen_socket_security_context(
		tls_certificate("tls/openssl/keys/server-certificate.pem"_path),
		tls_private_key("tls/openssl/keys/server-private-key.pem"_path)).value();

	auto const endpoint = test::generate_endpoint();
	auto const listen_socket = detail::listen<listen_socket_object, traits_type>(
		endpoint,
		server_security);

	auto connect_future = test::spawn([&]()
	{
		auto const client_security = openssl::create_socket_security_context().value();
		return detail::connect<socket_object, traits_type>(endpoint, client_security);
	});

	auto const server_socket = listen_socket.accept().socket;
	auto const client_socket = connect_future.get();

	signed char value = 42;
	REQUIRE(client_socket.write_some(as_write_buffer(&value, 1)) == 1);

	static_cast<volatile signed char&>(value) = 0;
	REQUIRE(server_socket.read_some(as_read_buffer(&value, 1)) == 1);
	REQUIRE(value == 42);
}

#if 0
TEST_CASE(
	"OpenSSL sockets can asynchronously connect and exchange data",
	"[openssl][socket][async]")
{
	using namespace senders;

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
#endif
