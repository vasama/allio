#include <allio/listen_handle.hpp>
#include <allio/socket_handle.hpp>

#include <allio/get_multiplexer.hpp>
#include <allio/path.hpp>
#include <allio/sync_wait.hpp>
#include <allio/task.hpp>
#include <allio/via.hpp>

#include <allio/test/shared_object.hpp>

#include <vsm/lazy.hpp>

#include <exec/env.hpp>
#include <exec/task.hpp>

#include <catch2/catch_all.hpp>

#include <filesystem>
#include <future>
#include <memory>
#include <type_traits>

using namespace allio;
namespace ex = stdexec;

using endpoint_type = test::shared_object<network_endpoint>;

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

	return endpoint_type(std::shared_ptr<network_endpoint>(vsm_move(ptr), &ptr->endpoint));
}

static endpoint_type make_ipv4_endpoint()
{
	ipv4_endpoint const endpoint(ipv4_address::localhost, 51234);
	return endpoint_type(std::make_shared<network_endpoint>(endpoint));
}

static endpoint_type make_ipv6_endpoint()
{
	ipv6_endpoint const endpoint(ipv6_address::localhost, 51234);
	return endpoint_type(std::make_shared<network_endpoint>(endpoint));
}

static endpoint_type generate_endpoint()
{
	return GENERATE(
		as<endpoint_type(*)()>()
		, make_local_endpoint
		, make_ipv4_endpoint
		, make_ipv6_endpoint
	)();
}


TEST_CASE("a stream socket can connect to a listening socket", "[socket_handle][blocking]")
{
	using socket_handle_tag = raw_socket_handle_t;
	using listen_handle_tag = raw_listen_handle_t;

	auto const endpoint = generate_endpoint();

	auto const listen_socket = listen_blocking<listen_handle_tag>(endpoint).value();

	auto connect_future = std::async(std::launch::async, [&]()
	{
		return connect_blocking<socket_handle_tag>(endpoint);
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


#if 0
template<typename Callable>
class my_io_callback : public detail::io_callback
{
	Callable m_callable;

public:
	my_io_callback(auto&& callable)
		: m_callable(vsm_forward(callable))
	{
	}

	void notify(detail::operation_base& operation, detail::io_status const status) noexcept override
	{
		m_callable(operation, status);
	}
};

template<typename Callable>
my_io_callback(Callable&&) -> my_io_callback<std::decay_t<Callable>>;

TEST_CASE("TEMP")
{
	using namespace detail;

	auto const endpoint = ipv4_endpoint(ipv4_address::localhost, 51234);

	auto multiplexer = default_multiplexer::create().value();
	auto listen_socket = raw_listen_blocking(endpoint).value().with_multiplexer(multiplexer).value();

	using operation_type = operation_t<default_multiplexer, raw_listen_handle_t, raw_listen_handle_t::accept_t>;

	std::optional<operation_type> operation;
	std::optional<vsm::result<decltype(listen_socket)::accept_result_type>> result;

	my_io_callback callback = [&](operation_base&, io_status const status)
	{
		auto r = notify_io<raw_listen_handle_t::accept_t>(listen_socket, *operation, status);

		if (r.has_value())
		{
			result = vsm_move(*r);
		}
		else if (r.is_pending())
		{
		}
		else
		{
			result = vsm::unexpected(r.error());
		}
	};

	operation.emplace(callback, io_args<raw_listen_handle_t::accept_t>()());
	REQUIRE(!submit_io<raw_listen_handle_t::accept_t>(listen_socket, *operation));
}
#endif


//TODO: Get rid of this when the MSVC bug is fixed.
#ifdef _MSC_VER
#	define allio_await_move std::move
#else
#	define allio_await_move
#endif

#if 1
TEST_CASE("", "[socket_handle][async]")
{
	auto const endpoint = generate_endpoint();

	auto multiplexer = default_multiplexer::create().value();

	sync_wait(multiplexer, [&]() -> task<void>
	{
		using S = decltype(raw_listen(endpoint));
		using E = decltype(exec::make_env(exec::with(get_multiplexer, default_multiplexer_handle(multiplexer))));
		static_assert(ex::sender_in<S, E>);

		auto const listen_socket = co_await via(multiplexer)(raw_listen(endpoint));
		//auto const listen_socket = co_await raw_listen(endpoint);
		typename decltype(listen_socket)::socket_handle_type server_socket(multiplexer);

		co_await ex::when_all(
			[&]() -> task<void>
			{
				auto& socket = server_socket;

				socket = allio_await_move(co_await listen_socket.accept()).socket;

				signed char request_data;
				REQUIRE(co_await socket.read_some(as_read_buffer(&request_data, 1)) == 1);

				signed char const reply_data = -request_data;
				REQUIRE(co_await socket.write_some(as_write_buffer(&reply_data, 1)) == 1);
			}(),
			[&]() -> task<void>
			{
				auto const socket = co_await via(multiplexer)(raw_connect(endpoint));
				//auto const socket = co_await raw_connect(endpoint);

				signed char const request_data = 42;
				REQUIRE(co_await socket.write_some(as_write_buffer(&request_data, 1)) == 1);

				signed char reply_data;
				REQUIRE(co_await socket.read_some(as_read_buffer(&reply_data, 1)) == 1);

				REQUIRE(reply_data == -42);
			}());
	}());
}
#endif

#if 0
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
		listen_socket_handle listen_socket = co_await error_into_except(listen_async(*multiplexer, address));
		stream_socket_handle server_socket;

		co_await unifex::when_all(
			[&]() -> unifex::task<void>
			{
				stream_socket_handle& socket = server_socket;

				socket = allio_await_move(co_await error_into_except(listen_socket.accept_async())).socket;
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
