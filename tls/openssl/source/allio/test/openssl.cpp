#include <allio/openssl/detail/openssl.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::detail;

static void pop_front_into(std::vector<std::byte>& vector, std::span<std::byte> const buffer)
{
	vsm_assert(buffer.size() <= vector.size());
	std::copy(vector.begin(), vector.begin() + buffer.size(), buffer.begin());
	vector.erase(vector.begin(), vector.begin() + buffer.size());
}

TEST_CASE("OpenSSL can asynchronously perform a TLS handshake", "[openssl]")
{
	using stream = std::vector<std::byte>;

	auto const handshake = [&](
		stream& read_stream,
		stream& write_stream,
		openssl_state_base& state,
		auto const p_member)
	{
		if (state.m_want_read)
		{
			if (read_stream.empty())
			{
				return true;
			}

			size_t const transfer_size = std::min(
				read_stream.size(),
				static_cast<size_t>(state.m_r_end - state.m_r_beg));

			std::byte* const new_r_pos = state.m_r_end - transfer_size;
			pop_front_into(read_stream, std::span(new_r_pos, state.m_r_end));
			state.m_r_pos = new_r_pos;

			state.m_want_read = false;
		}

		auto const r = (state.*p_member)().value();

		if (state.m_want_write)
		{
			write_stream.append_range(std::span(state.m_w_beg, state.m_w_pos));
			state.m_w_pos = state.m_w_beg;

			state.m_want_write = false;
		}

		return !r.has_value();
	};

	using namespace path_literals;
	auto const server_ssl_ctx = openssl_create_server_ssl_ctx(
		make_args<security_context_parameters>(
			tls_certificate("tls/openssl/keys/server-certificate.pem"_path),
			tls_private_key("tls/openssl/keys/server-private-key.pem"_path)
		)).value();
	auto const client_ssl_ctx = openssl_create_client_ssl_ctx(
		make_args<security_context_parameters>(
		)).value();


	openssl_state_base server_state;
	openssl_state_base client_state;

	server_state.m_ssl = server_state.create_ssl(server_ssl_ctx.get()).value();
	client_state.m_ssl = client_state.create_ssl(client_ssl_ctx.get()).value();


	static constexpr size_t buffer_size = 64;

	std::byte server_r_buffer[buffer_size];
	server_state.m_r_beg = server_r_buffer;
	server_state.m_r_pos = server_r_buffer + buffer_size;
	server_state.m_r_end = server_r_buffer + buffer_size;

	std::byte server_w_buffer[buffer_size];
	server_state.m_w_beg = server_w_buffer;
	server_state.m_w_pos = server_w_buffer;
	server_state.m_w_end = server_w_buffer + buffer_size;

	std::byte client_r_buffer[buffer_size];
	client_state.m_r_beg = client_r_buffer;
	client_state.m_r_pos = client_r_buffer + buffer_size;
	client_state.m_r_end = client_r_buffer + buffer_size;

	std::byte client_w_buffer[buffer_size];
	client_state.m_w_beg = client_w_buffer;
	client_state.m_w_pos = client_w_buffer;
	client_state.m_w_end = client_w_buffer + buffer_size;


	stream client_to_server;
	stream server_to_client;

	bool accept_pending = true;
	bool connect_pending = true;

	while (accept_pending || connect_pending)
	{
		if (accept_pending)
		{
			accept_pending = handshake(
				client_to_server,
				server_to_client,
				server_state,
				&openssl_state_base::accept);
		}

		if (connect_pending)
		{
			connect_pending = handshake(
				server_to_client,
				client_to_server,
				client_state,
				&openssl_state_base::connect);
		}
	}
}