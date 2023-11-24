#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/completion_port.hpp>
#include <allio/impl/win32/wsa_ex.hpp>
#include <allio/test/spawn.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::posix;
using namespace allio::win32;

TEST_CASE("WSA supports unix stream sockets", "[windows][wsa][socket]")
{
	REQUIRE(posix::create_socket(AF_UNIX, SOCK_STREAM, 0, false));
}

TEST_CASE("WSA async connect and accept", "[windows][wsa][socket][async]")
{
	auto const addr = socket_address::make(ipv4_endpoint(ipv4_address::localhost, 51234)).value();

	auto const create_socket = [&]()
	{
		return posix::create_socket(
			addr.addr.sa_family,
			SOCK_STREAM,
			IPPROTO_TCP,
			/* multiplexable: */ true).value().socket;
	};

	auto const completion_port = create_completion_port(1).value();


	/* accept */

	auto const listen_socket = create_socket();

	set_completion_information(
		(HANDLE)listen_socket.get(),
		completion_port.get(),
		/* key_context: */ nullptr).value();

	posix::socket_listen(listen_socket.get(), addr, nullptr).value();

	auto const server_socket = create_socket();
	wsa_accept_address_buffer accept_addr;

	OVERLAPPED accept_overlapped = {};
	REQUIRE(WSA_IO_PENDING == wsa_accept_ex(listen_socket.get(), server_socket.get(), accept_addr, accept_overlapped));


	/* connect */

	auto const client_socket = create_socket();

	set_completion_information(
		(HANDLE)client_socket.get(),
		completion_port.get(),
		/* key_context: */ nullptr).value();

	// The socket must be bound before calling ConnectEx.
	{
		posix::socket_address_union bind_addr;
		memset(&bind_addr, 0, sizeof(bind_addr));
		bind_addr.addr.sa_family = addr.addr.sa_family;

		socket_bind(client_socket.get(), bind_addr, addr.size).value();
	}

	OVERLAPPED connect_overlapped = {};
	REQUIRE(WSA_IO_PENDING == wsa_connect_ex(client_socket.get(), addr, connect_overlapped));


	/* handle completions */

	FILE_IO_COMPLETION_INFORMATION completions[3];

	auto const get_completions = [&](size_t const index, size_t const count)
	{
		return std::span(completions).subspan(index, count);
	};

	REQUIRE(remove_io_completions(completion_port.get(), get_completions(0, 1), std::chrono::milliseconds(100)).value() == 1);
	REQUIRE(remove_io_completions(completion_port.get(), get_completions(1, 2), std::chrono::milliseconds(100)).value() == 1);

	auto const get_completion = [&](OVERLAPPED const& overlapped) -> FILE_IO_COMPLETION_INFORMATION const&
	{
		return completions[0].ApcContext == &overlapped ? completions[0] : completions[1];
	};

	// accept completion
	{
		auto const& completion = get_completion(accept_overlapped);
		REQUIRE(completion.ApcContext == &accept_overlapped);
		REQUIRE(NT_SUCCESS(completion.IoStatusBlock.Status));
	}

	// connect completion
	{
		auto const& completion = get_completion(connect_overlapped);
		REQUIRE(completion.ApcContext == &connect_overlapped);
		REQUIRE(NT_SUCCESS(completion.IoStatusBlock.Status));
	}
}


TEST_CASE("WSA does not support unix datagram sockets", "[windows][datagram_socket][wsa]")
{
	// The purpose of this test is to quickly detect future support for this feature.
	REQUIRE(!posix::create_socket(AF_UNIX, SOCK_DGRAM, 0, false));
}

TEST_CASE("WSA blocking datagram send and receive", "[windows][wsa][datagram_socket][blocking]")
{
	auto const create_socket = [&]()
	{
		return posix::create_socket(
			AF_INET,
			SOCK_DGRAM,
			IPPROTO_UDP,
			/* multiplexable: */ false).value().socket;
	};

	auto const receive_socket = create_socket();

	auto const addr = socket_address::make(
		ipv4_endpoint(ipv4_address::localhost, 51234)).value();

	socket_bind(receive_socket.get(), addr).value();

	signed char receive_buffer = 0;
	socket_address receive_addr;
	receive_addr.size = sizeof(socket_address_union);

	auto receive_future = test::spawn([&]()
	{
		int const r = recvfrom(
			receive_socket.get(),
			reinterpret_cast<char*>(&receive_buffer),
			1,
			0,
			&receive_addr.addr,
			&receive_addr.size);
		return r;
	});

	{
		auto const send_socket = create_socket();

		signed char send_buffer = 42;
		int const r = sendto(
			send_socket.get(),
			reinterpret_cast<char*>(&send_buffer),
			1,
			0,
			&addr.addr,
			addr.size);
		REQUIRE(r == 1);
	}

	REQUIRE(receive_future.get() == 1);
	REQUIRE(receive_buffer == 42);
}

TEST_CASE("WSA asynchronous datagram send and receive", "[windows][wsa][datagram_socket][async]")
{
	auto const addr = socket_address::make(ipv4_endpoint(ipv4_address::localhost, 51234)).value();

	auto const create_socket = [&]()
	{
		return posix::create_socket(
			addr.addr.sa_family,
			SOCK_DGRAM,
			IPPROTO_UDP,
			/* multiplexable: */ true).value().socket;
	};

	auto const completion_port = create_completion_port(1).value();


	/* receive */

	auto const receive_socket = create_socket();
	socket_bind(receive_socket.get(), addr).value();

	set_completion_information(
		(HANDLE)receive_socket.get(),
		completion_port.get(),
		/* key_context: */ nullptr).value();

	signed char receive_value = 0;

	socket_address receive_addr;
	receive_addr.size = sizeof(socket_address_union);

	OVERLAPPED receive_overlapped = {};
	{
		WSABUF buf =
		{
			.len = 1,
			.buf = reinterpret_cast<char*>(&receive_value),
		};
		DWORD transferred;
		DWORD flags = 0;

		int const r = WSARecvFrom(
			receive_socket.get(),
			&buf,
			1,
			&transferred,
			&flags,
			&receive_addr.addr,
			&receive_addr.size,
			&receive_overlapped,
			/* lpCompletionRoutine: */ nullptr);
		REQUIRE(r == SOCKET_ERROR);
		REQUIRE(WSAGetLastError() == ERROR_IO_PENDING);
	}


	/* send */

	auto const send_socket = create_socket();

	set_completion_information(
		(HANDLE)send_socket.get(),
		completion_port.get(),
		/* key_context: */ nullptr).value();

	signed char send_value = 42;

	OVERLAPPED send_overlapped = {};
	{
		WSABUF buf =
		{
			.len = 1,
			.buf = reinterpret_cast<char*>(&send_value),
		};
		DWORD transferred;

		int const r = WSASendTo(
			send_socket.get(),
			&buf,
			1,
			&transferred,
			/* dwFlags: */ 0,
			&addr.addr,
			addr.size,
			&send_overlapped,
			/* lpCompletionRoutine: */ nullptr);

		if (r == SOCKET_ERROR)
		{
			REQUIRE(WSAGetLastError() == ERROR_IO_PENDING);
		}
	}
	

	/* handle completions */

	FILE_IO_COMPLETION_INFORMATION completions[3];

	auto const get_completions = [&](size_t const index, size_t const count)
	{
		return std::span(completions).subspan(index, count);
	};

	REQUIRE(remove_io_completions(completion_port.get(), get_completions(0, 1), std::chrono::milliseconds(100)).value() == 1);
	REQUIRE(remove_io_completions(completion_port.get(), get_completions(1, 2), std::chrono::milliseconds(100)).value() == 1);

	auto const get_completion = [&](OVERLAPPED const& overlapped) -> FILE_IO_COMPLETION_INFORMATION const&
	{
		return completions[0].ApcContext == &overlapped ? completions[0] : completions[1];
	};

	// receive completion
	{
		auto const& completion = get_completion(receive_overlapped);
		REQUIRE(completion.ApcContext == &receive_overlapped);
		REQUIRE(NT_SUCCESS(completion.IoStatusBlock.Status));
		REQUIRE(completion.IoStatusBlock.Information == 1);
	}

	// send completion
	{
		auto const& completion = get_completion(send_overlapped);
		REQUIRE(completion.ApcContext == &send_overlapped);
		REQUIRE(NT_SUCCESS(completion.IoStatusBlock.Status));
		REQUIRE(completion.IoStatusBlock.Information == 1);
	}
}
