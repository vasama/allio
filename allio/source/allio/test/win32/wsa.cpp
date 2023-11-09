#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/completion_port.hpp>
#include <allio/impl/win32/wsa_ex.hpp>
#include <allio/socket_handle.hpp>

#include <catch2/catch_all.hpp>

#include <future>

using namespace allio;
using namespace allio::posix;
using namespace allio::win32;

TEST_CASE("WSA AcceptEx/ConnectEx", "[wsa][windows]")
{
	auto const addr = socket_address::make(ipv4_endpoint(ipv4_address::localhost, 51234)).value();

	auto const completion_port = create_completion_port(1).value();


	/* accept */

	auto const listen_socket = posix::create_socket(addr.addr.sa_family, true).value().socket;

	set_completion_information(
		(HANDLE)listen_socket.get(),
		completion_port.get(),
		/* key_context: */ nullptr).value();

	posix::socket_listen(listen_socket.get(), addr, nullptr).value();

	auto const server_socket = posix::create_socket(addr.addr.sa_family, true).value().socket;
	wsa_accept_address_buffer accept_addr;

	OVERLAPPED accept_overlapped = {};
	REQUIRE(WSA_IO_PENDING == wsa_accept_ex(listen_socket.get(), server_socket.get(), accept_addr, accept_overlapped));


	/* connect */

	auto const client_socket = posix::create_socket(addr.addr.sa_family, true).value().socket;

	set_completion_information(
		(HANDLE)client_socket.get(),
		completion_port.get(),
		/* key_context: */ nullptr).value();

	// The socket must be bound before calling ConnectEx.
	{
		posix::socket_address_union bind_addr;
		memset(&bind_addr, 0, sizeof(bind_addr));
		bind_addr.addr.sa_family = addr.addr.sa_family;

		REQUIRE(::bind(client_socket.get(), &bind_addr.addr, addr.size) != SOCKET_ERROR);
	}

	OVERLAPPED connect_overlapped = {};
	REQUIRE(WSA_IO_PENDING == wsa_connect_ex(client_socket.get(), addr, connect_overlapped));


	/* handle completions */

	FILE_IO_COMPLETION_INFORMATION completions[3];

	REQUIRE(remove_io_completions(completion_port.get(), completions, deadline::instant()).value() == 2);

	// accept completion
	{
		FILE_IO_COMPLETION_INFORMATION& accept_completion =
			completions[0].ApcContext == &accept_overlapped ? completions[0] : completions[1];

		REQUIRE(accept_completion.ApcContext == &accept_overlapped);
		REQUIRE(NT_SUCCESS(accept_completion.IoStatusBlock.Status));
	}

	// connect completion
	{
		FILE_IO_COMPLETION_INFORMATION& connect_completion =
			completions[0].ApcContext == &connect_overlapped ? completions[0] : completions[1];

		REQUIRE(connect_completion.ApcContext == &connect_overlapped);
		REQUIRE(NT_SUCCESS(connect_completion.IoStatusBlock.Status));
	}
}
