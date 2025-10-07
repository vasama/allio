#include <allio/openssl/listen_socket.hpp>
#include <allio/openssl/socket.hpp>

#include <allio/blocking/traits.hpp>
#include <allio/senders/sync_wait.hpp>
#include <allio/senders/task.hpp>
#include <allio/test/network.hpp>
#include <allio/test/spawn.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::openssl;

namespace ex = stdexec;

static auto make_server_security_context()
{
	using namespace path_literals;

	return openssl::create_listen_socket_security_context(
		tls_certificate(allio_test_secret_path "/server-certificate.pem"_path),
		tls_private_key(allio_test_secret_path "/server-private-key.pem"_path)).value();
}

static auto make_client_security_context()
{
	return openssl::create_socket_security_context().value();
}

TEST_CASE(
	"Blocking OpenSSL stream sockets can exchange data",
	"[openssl][socket][blocking]")
{
	using namespace blocking;

	auto const endpoint = test::generate_endpoint();
	auto const listen_socket = detail::listen<openssl::listen_socket_t, traits_type>(
		endpoint,
		make_server_security_context());

	auto connect_future = test::spawn([&]()
	{
		return detail::connect<openssl::socket_t, traits_type>(
			endpoint,
			make_client_security_context());
	});

	auto const server_socket = listen_socket.accept();
	auto const client_socket = connect_future.get();

	char const write_buffer[] =
		"Lorem ipsum dolor sit amet, consectetur adipisci elit, sed eiusmod tempor incidunt ut "
		"labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco "
		"laboris nisi ut aliquid ex ea commodi consequat. Quis aute iure reprehenderit in "
		"voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint obcaecat "
		"cupiditat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

	client_socket.write(as_write_buffer(write_buffer));

	char read_buffer[sizeof(write_buffer)];
	server_socket.read(as_read_buffer(read_buffer));

	REQUIRE(std::string_view(read_buffer) == std::string_view(write_buffer));

#if 0 // TODO: Enable this. Requires match_error from allio/source/test...
	REQUIRE_THROWS_MATCHES(
		client_socket.read_some(as_read_buffer(read_buffer), deadline::instant()),
		std::system_error,
		match_error(std::errc::timed_out));
#endif
}

TEST_CASE(
	"Asynchronous OpenSSL stream sockets can exchange data",
	"[openssl][socket][senders]")
{
	using namespace senders;

	auto multiplexer = default_multiplexer::create().value();

	auto const combined_task = []() -> task<void>
	{
		auto const endpoint = test::generate_endpoint();

		// Make sure the listening socket is bound before the client attempts to connect.
		auto const listen_socket = co_await detail::listen<openssl::listen_socket_t, traits_type>(
			endpoint,
			make_server_security_context());

		auto const server_task = [&]() -> task<void>
		{
			auto const socket = co_await listen_socket.accept();

			// Wait for a request from the client:
			signed char request_data;
			size_t const rs = co_await socket.read_some(as_read_buffer(&request_data, 1));
			REQUIRE(rs == 1);

			// Send a reply to the client:
			signed char const reply_data = -request_data;
			size_t const ws = co_await socket.write_some(as_write_buffer(&reply_data, 1));
			REQUIRE(ws == 1);
		};

		auto const client_task = [&]() -> task<void>
		{
			// Connect to the server:
			auto const socket = co_await detail::connect<openssl::socket_t, traits_type>(
				endpoint,
				make_client_security_context());

			// Send a request to the server:
			signed char const request_data = 42;
			size_t const ws = co_await socket.write_some(as_write_buffer(&request_data, 1));
			REQUIRE(ws == 1);

			// Wait for a reply from the server:
			signed char reply_data;
			size_t const rs = co_await socket.read_some(as_read_buffer(&reply_data, 1));
			REQUIRE(rs == 1);

			REQUIRE(reply_data == -42);
		};

		co_await ex::when_all(server_task(), client_task());
	};

	senders::sync_wait(multiplexer, combined_task());
}
