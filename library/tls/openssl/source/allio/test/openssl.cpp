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
		openssl_tunnel_state& tunnel,
		auto const p_member,
		auto&&... args)
	{
		if (tunnel.read_needed())
		{
			if (read_stream.empty())
			{
				return true;
			}

			auto const buffer = tunnel.get_read_buffer();
			size_t const transfer_size = std::min(buffer.size(), read_stream.size());
			std::memcpy(buffer.data(), read_stream.data(), transfer_size);
			tunnel.read_completed(transfer_size);

			read_stream.erase(
				read_stream.begin(),
				read_stream.begin() + static_cast<ptrdiff_t>(transfer_size));
		}

		auto const r = (tunnel.*p_member)(vsm_forward(args)...).value();

		if (tunnel.write_needed())
		{
			auto const buffer = tunnel.get_write_buffer();
			write_stream.insert(write_stream.end(), buffer.begin(), buffer.end());
			tunnel.write_completed(buffer.size());
		}

		return !r.has_value();
	};

	using namespace path_literals;
	auto const server_ssl_ctx = openssl_create_server_ssl_ctx(
		make_args<security_context_parameters>(
			tls_certificate = allio_test_secret_path "/server-certificate.pem"_path,
			tls_private_key = allio_test_secret_path "/server-private-key.pem"_path)).value();

	auto const client_ssl_ctx = openssl_create_client_ssl_ctx(
		make_args<security_context_parameters>(
			tls_peer_certificate = allio_test_secret_path "/server-certificate.pem"_path)).value();


	openssl_tunnel_state server_socket;
	openssl_tunnel_state client_socket;

	server_socket.initialize(tunnel_mode::server, server_ssl_ctx.get()).value();
	client_socket.initialize(tunnel_mode::client, client_ssl_ctx.get()).value();

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


	bool server_connect_pending = true;
	bool client_connect_pending = true;

	while (server_connect_pending || client_connect_pending)
	{
		if (server_connect_pending)
		{
			server_connect_pending = enter_server(
				&openssl_tunnel_state::connect,
				tunnel_mode::server);
		}

		if (client_connect_pending)
		{
			client_connect_pending = enter_client(
				&openssl_tunnel_state::connect,
				tunnel_mode::client);
		}
	}

	char const write_buffer[] =
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
				&openssl_tunnel_state::read_some,
				as_read_buffer(read_buffer));
		}

		if (write_pending)
		{
			write_pending = enter_client(
				&openssl_tunnel_state::write_some,
				as_write_buffer(write_buffer));
		}
	}

	// TODO: Test disconnect as well.

	REQUIRE(std::memcmp(read_buffer, write_buffer, sizeof(read_buffer)) == 0);
}
