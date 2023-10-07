#include <allio/listen_socket_handle.hpp>
#include <allio/stream_socket_handle.hpp>

#include <allio/path.hpp>
#include <allio/sync_wait.hpp>

#include <vsm/lazy.hpp>

#include <catch2/catch_all.hpp>

#include <filesystem>
#include <future>
#include <memory>
#include <type_traits>

using namespace allio;

namespace {
template<typename T>
class shared_object
{
	std::shared_ptr<T> m_ptr;

public:
	template<std::convertible_to<T> U = T>
	shared_object(U&& value)
		: m_ptr(std::make_shared<T>(vsm_forward(value)))
	{
	}

	explicit shared_object(std::shared_ptr<T> ptr)
		: m_ptr(vsm_move(ptr))
	{
	}

	operator T const&() const
	{
		return *m_ptr;
	}

	T const* operator->() const
	{
		return m_ptr.get();
	}
};

} // namespace


using endpoint_type = shared_object<network_endpoint>;

// Use shared_ptr with the aliasing constructor to hide
// the storage used for the local_address endpoint path.
static endpoint_type make_local_endpoint()
{
	struct storage
	{
		path endpoint_path;
		network_endpoint endpoint;
	};

	auto file_path = std::filesystem::temp_directory_path() / "allio.sock";
	std::filesystem::remove(file_path);

	auto ptr = std::make_shared<storage>(vsm_lazy(storage
	{
		.endpoint_path = path(vsm_move(file_path).string()),
	}));

	ptr->endpoint = local_address(ptr->endpoint_path);

	return shared_object(std::shared_ptr<network_endpoint>(vsm_move(ptr), &ptr->endpoint));
}

static endpoint_type make_ipv4_endpoint()
{
	ipv4_endpoint const endpoint(ipv4_address::localhost, 51234);
	return shared_object(std::make_shared<network_endpoint>(endpoint));
}

static endpoint_type generate_endpoint()
{
	return GENERATE(
		as<endpoint_type(*)()>()
		, make_local_endpoint
		, make_ipv4_endpoint
	)();
}


TEST_CASE("a stream socket can connect to a listening socket", "[socket_handle][blocking]")
{
	auto const endpoint = generate_endpoint();

	auto const socket_listen = listen(endpoint).value();

	auto connect_future = std::async(std::launch::async, [&]()
	{
		return connect(endpoint);
	});

	auto const server_socket = socket_listen.accept().value().socket;
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


#if 0
//TODO: Get rid of this when the MSVC bug is fixed.
#ifdef _MSC_VER
#	define allio_await_move(...) ::std::move(__VA_ARGS__)
#else
#	define allio_await_move(...) (__VA_ARGS__)
#endif


static path get_temp_path(std::string_view const name)
{
	return path((std::filesystem::temp_directory_path() / name).string());
}

TEST_CASE("stream_socket_handle localhost server & client", "[socket_handle][stream_socket_handle]")
{
	path const socket_path = get_temp_path("allio-test-socket");

	network_endpoint const address = GENERATE(1, 0)
		? network_endpoint(local_address(socket_path))
		: network_endpoint(ipv4_endpoint{ ipv4_address::localhost, 51234 });

	if (address.kind() == network_address_kind::local)
	{
		std::filesystem::remove(std::filesystem::path(socket_path.string()));
	}

	unique_multiplexer_ptr const multiplexer = create_default_multiplexer().value();

	(void)sync_wait(*multiplexer, [&]() -> unifex::task<void>
	{
		listen_socket_handle socket_listen = co_await error_into_except(listen_async(*multiplexer, address));
		stream_socket_handle server_socket;

		co_await unifex::when_all(
			[&]() -> unifex::task<void>
			{
				stream_socket_handle& socket = server_socket;

				socket = allio_await_move(co_await error_into_except(socket_listen.accept_async())).socket;
				socket.set_multiplexer(multiplexer.get());

				int request_data;
				REQUIRE(co_await error_into_except(socket.read_async(as_read_buffer(&request_data, 1))) == sizeof(request_data));

				int const reply_data = -request_data;
				REQUIRE(co_await error_into_except(socket.write_async(as_write_buffer(&reply_data, 1))) == sizeof(reply_data));
			}(),

			[&]() -> unifex::task<void>
			{
				stream_socket_handle socket = co_await error_into_except(connect_async(*multiplexer, address));

				int const request_data = 42;
				REQUIRE(co_await error_into_except(socket.write_async(as_write_buffer(&request_data, 1))) == sizeof(request_data));

				int reply_data;
				REQUIRE(co_await error_into_except(socket.read_async(as_read_buffer(&reply_data, 1))) == sizeof(reply_data));

				REQUIRE(reply_data == -request_data);
			}()
		);
	}()).value();
}
#endif
