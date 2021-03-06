#include <allio/socket_handle_async.hpp>

#include <allio/default_multiplexer.hpp>
#include <allio/path.hpp>
#include <allio/sync_wait.hpp>

#include <unifex/task.hpp>
#include <unifex/when_all.hpp>

#include <catch2/catch_all.hpp>

#include <filesystem>
#include <type_traits>

using namespace allio;

static path get_temp_path(std::string_view const name)
{
	return path((std::filesystem::temp_directory_path() / name).string());
}

TEST_CASE("socket_handle localhost server & client", "[socket_handle]")
{
	path const socket_path = get_temp_path("allio-test-socket");

	network_address const address = GENERATE(1, 0)
		? network_address(local_address(socket_path))
		: network_address(ipv4_address::localhost(51234));

	if (address.kind() == network_address_kind::local)
	{
		std::filesystem::remove(std::filesystem::path(socket_path.string()));
	}

	unique_multiplexer const multiplexer = create_default_multiplexer().value();

	sync_wait(*multiplexer, [&]() -> unifex::task<void>
	{
		listen_socket_handle listen_socket = co_await listen_async(*multiplexer, address);

		socket_handle server_socket;

		co_await unifex::when_all(
			[&]() -> unifex::task<void>
			{
				socket_handle& socket = server_socket;

#ifdef _MSC_VER // TODO: Get rid of this when the MSVC bug is fixed.
				socket = std::move((co_await listen_socket.accept_async()).socket);
#else
				socket = (co_await listen_socket.accept_async()).socket;
#endif
				socket.set_multiplexer(multiplexer.get());

				int request_data;
				REQUIRE(co_await socket.read_async(as_read_buffer(&request_data, 1)) == sizeof(request_data));

				int const reply_data = -request_data;
				REQUIRE(co_await socket.write_async(as_write_buffer(&reply_data, 1)) == sizeof(reply_data));
			}(),

			[&]() -> unifex::task<void>
			{
				socket_handle socket = co_await connect_async(*multiplexer, address);

				int const request_data = 42;
				REQUIRE(co_await socket.write_async(as_write_buffer(&request_data, 1)) == sizeof(request_data));

				int reply_data;
				REQUIRE(co_await socket.read_async(as_read_buffer(&reply_data, 1)) == sizeof(reply_data));

				REQUIRE(reply_data == -request_data);
			}()
		);
	}()).value();
}
