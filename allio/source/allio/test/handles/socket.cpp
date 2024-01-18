#include <allio/blocking/raw_listen_socket.hpp>
#include <allio/blocking/raw_socket.hpp>
#include <allio/senders/raw_listen_socket.hpp>
#include <allio/senders/raw_socket.hpp>

#include <allio/sync_wait.hpp>
#include <allio/task.hpp>
#include <allio/test/network.hpp>
#include <allio/test/spawn.hpp>

#include <catch2/catch_all.hpp>

#include <format>

using namespace allio;
namespace ex = stdexec;

namespace {

class match_error
{
	std::error_condition m_error_condition;

public:
	explicit match_error(std::error_condition const error_condition)
		: m_error_condition(error_condition)
	{
	}

	bool match(std::system_error const& e) const
	{
		return e.code().default_error_condition() == m_error_condition;
	}

	std::string toString() const
	{
		return std::format("Equals: '{}'", m_error_condition.message());
	}
};

} // namespace

TEST_CASE("a stream socket can connect to a listening socket and exchange data", "[socket][blocking]")
{
	using namespace blocking;

	auto const endpoint = test::generate_endpoint();
	auto const listen_socket = raw_listen(endpoint);

	auto connect_future = test::spawn([&]()
	{
		return raw_connect(endpoint);
	});

	auto const server_socket = listen_socket.accept().socket;
	auto const client_socket = connect_future.get();

	SECTION("The server socket has no data to read")
	{
		signed char value = 0;
		REQUIRE_THROWS_MATCHES(
			(void)server_socket.read_some(as_read_buffer(&value, 1), deadline::instant()),
			std::system_error,
			match_error(std::errc::timed_out));
	}

	SECTION("The client socket has no data to read")
	{
		signed char value = 0;
		REQUIRE_THROWS_MATCHES(
			(void)client_socket.read_some(as_read_buffer(&value, 1), deadline::instant()),
			std::system_error,
			match_error(std::errc::timed_out));
	}

	SECTION("The client can send data to the server")
	{
		signed char value = 42;
		REQUIRE(client_socket.write_some(as_write_buffer(&value, 1)) == 1);

		static_cast<volatile signed char&>(value) = 0;
		REQUIRE(server_socket.read_some(as_read_buffer(&value, 1)) == 1);
		REQUIRE(value == 42);
	}

	SECTION("The server can send data to the client")
	{
		signed char value = 42;
		REQUIRE(server_socket.write_some(as_write_buffer(&value, 1)) == 1);

		static_cast<volatile signed char&>(value) = 0;
		REQUIRE(client_socket.read_some(as_read_buffer(&value, 1)) == 1);
		REQUIRE(value == 42);
	}
}

TEST_CASE("a stream socket can asynchronously connect to a listening socket and exchange data", "[socket][async]")
{
	using namespace senders;

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
