#include <allio/openssl/detail/openssl.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::detail;

TEST_CASE("OpenSSL can asynchronously perform a TLS handshake", "[openssl]")
{
	using stream = std::vector<std::byte>;

	auto const enter = [&](
		stream& read_stream,
		stream& write_stream,
		openssl_socket& socket,
		auto const p_member,
		auto&&... args)
	{
		if (socket.want_read())
		{
			if (read_stream.empty())
			{
				return true;
			}

			auto const buffer = socket.get_read_buffer();
			size_t const transfer_size = std::min(buffer.size(), read_stream.size());
			std::memcpy(buffer.data(), read_stream.data(), transfer_size);
			socket.read_completed(transfer_size);

			read_stream.erase(
				read_stream.begin(),
				read_stream.begin() + static_cast<ptrdiff_t>(transfer_size));
		}

		auto const r = (socket.*p_member)(vsm_forward(args)...).value();

		if (socket.want_write())
		{
			auto const buffer = socket.get_write_buffer();
			write_stream.insert(write_stream.end(), buffer.begin(), buffer.end());
			socket.write_completed(buffer.size());
		}

		return !r.has_value();
	};

	using namespace path_literals;
	auto const server_ssl_ctx = openssl_create_server_ssl_ctx(
		make_args<security_context_parameters>(
			tls_certificate(allio_test_secret_path "/server-certificate.pem"_path),
			tls_private_key(allio_test_secret_path "/server-private-key.pem"_path))).value();

	auto const client_ssl_ctx = openssl_create_client_ssl_ctx(
		make_args<security_context_parameters>()).value();


	openssl_socket server_socket;
	openssl_socket client_socket;

	server_socket.initialize(server_ssl_ctx.get()).value();
	client_socket.initialize(client_ssl_ctx.get()).value();

	stream client_to_server;
	stream server_to_client;

	auto const enter_server = [&](auto const p_member, auto&&... args) -> bool
	{
		return enter(
			client_to_server,
			server_to_client,
			server_socket,
			p_member,
			vsm_forward(args)...);
	};

	auto const enter_client = [&](auto const p_member, auto&&... args) -> bool
	{
		return enter(
			server_to_client,
			client_to_server,
			client_socket,
			p_member,
			vsm_forward(args)...);
	};


	static constexpr size_t buffer_size = 64;

	std::byte server_read_buffer[buffer_size];
	server_socket.set_read_buffer(server_read_buffer);

	std::byte server_write_buffer[buffer_size];
	server_socket.set_write_buffer(server_write_buffer);

	std::byte client_read_buffer[buffer_size];
	client_socket.set_read_buffer(client_read_buffer);

	std::byte client_write_buffer[buffer_size];
	client_socket.set_write_buffer(client_write_buffer);


	bool accept_pending = true;
	bool connect_pending = true;

	while (accept_pending || connect_pending)
	{
		if (accept_pending)
		{
			accept_pending = enter_server(&openssl_socket::accept);
		}

		if (connect_pending)
		{
			connect_pending = enter_client(&openssl_socket::connect);
		}
	}

	char write_buffer[] =
		"Lorem ipsum dolor sit amet, consectetur adipisci elit, sed eiusmod tempor incidunt ut "
		"labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco "
		"laboris nisi ut aliquid ex ea commodi consequat. Quis aute iure reprehenderit in "
		"voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint obcaecat "
		"cupiditat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

	char read_buffer[sizeof(write_buffer)] = {};

	bool read_pending = true;
	bool write_pending = true;

	while (read_pending || write_pending)
	{
		if (read_pending)
		{
			read_pending = enter_server(
				&openssl_socket::read_some,
				as_read_buffer(read_buffer, sizeof(read_buffer)));
		}

		if (write_pending)
		{
			write_pending = enter_client(
				&openssl_socket::write_some,
				as_write_buffer(write_buffer, sizeof(write_buffer)));
		}
	}

	REQUIRE(std::memcmp(read_buffer, write_buffer, sizeof(read_buffer)) == 0);
}
