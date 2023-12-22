#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/completion_port.hpp>
#include <allio/impl/win32/wsa.hpp>
#include <allio/test/spawn.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::posix;
using namespace allio::win32;

static constexpr DWORD SIO_POLL = _WSAIORW(IOC_WS2, 31);


//TODO: Add using in a public header.
using detail::io_flags;

TEST_CASE("WSA supports unix stream sockets", "[windows][wsa][socket]")
{
	REQUIRE(posix::create_socket(AF_UNIX, SOCK_STREAM, 0, io_flags::none));
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
			io_flags::create_non_blocking).value().socket;
	};

	auto const completion_port = create_completion_port(1).value();


	// Accept

	auto const listen_socket = create_socket();

	set_completion_information(
		(HANDLE)listen_socket.get(),
		completion_port.get(),
		/* key_context: */ nullptr).value();

	posix::socket_listen(listen_socket.get(), addr, 0).value();

	auto const server_socket = create_socket();
	wsa_accept_address_buffer accept_addr;

	OVERLAPPED accept_overlapped = {};
	REQUIRE(WSA_IO_PENDING == wsa_accept_ex(listen_socket.get(), server_socket.get(), accept_addr, accept_overlapped));


	// Connect

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


	// Handle completions
	{
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

		// Accept completion
		{
			auto const& completion = get_completion(accept_overlapped);
			REQUIRE(completion.ApcContext == &accept_overlapped);
			REQUIRE(NT_SUCCESS(completion.IoStatusBlock.Status));
		}

		// Connect completion
		{
			auto const& completion = get_completion(connect_overlapped);
			REQUIRE(completion.ApcContext == &connect_overlapped);
			REQUIRE(NT_SUCCESS(completion.IoStatusBlock.Status));
		}
	}

#if 0
	SECTION("Asynchoronous polling")
	{
		struct AFD_HANDLE
		{
			SOCKET Handle;
			ULONG Events;
			NTSTATUS Status;
		};
	
		struct AFD_POLL_INFO
		{
			LARGE_INTEGER Timeout;
			ULONG HandleCount;
			ULONG_PTR Exclusive;
			AFD_HANDLE Handles[1];
		};

		AFD_POLL_INFO poll_info;
		poll_info.Timeout.QuadPart = std::numeric_limits<int64_t>::max();
		poll_info.HandleCount = 1;
		poll_info.Exclusive = FALSE;
		poll_info.Handles[0].Handle = server_socket.get();
		poll_info.Handles[0].Events = 1 /*AFD_EVENT_RECEIVE*/;

		HANDLE event;
		NtCreateEvent(
			&event,
			EVENT_ALL_ACCESS,
			NULL,
			SynchronizationEvent,
			FALSE);

		IO_STATUS_BLOCK io_status_block = make_io_status_block();
		NTSTATUS const status = win32::NtDeviceIoControlFile(
			(HANDLE)server_socket.get(),
			event,
			/* ApcRoutine: */ nullptr,
			/* ApcContext: */ nullptr,
			&io_status_block,
			0x12024 /*IOCTL_AFD_SELECT*/,
			&poll_info,
			sizeof(poll_info),
			&poll_info,
			sizeof(poll_info));

		int x = 0;
	}
#endif
}


TEST_CASE("WSA does not support unix datagram sockets", "[windows][datagram_socket][wsa]")
{
	// The purpose of this test is to quickly detect future support for this feature.
	REQUIRE(!posix::create_socket(AF_UNIX, SOCK_DGRAM, 0, io_flags::none));
}

TEST_CASE("WSA blocking datagram send and receive", "[windows][wsa][datagram_socket][blocking]")
{
	auto const create_socket = [&]()
	{
		return posix::create_socket(
			AF_INET,
			SOCK_DGRAM,
			IPPROTO_UDP,
			io_flags::none).value().socket;
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
			io_flags::create_non_blocking).value().socket;
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


#if 0
TEST_CASE("WSA RIO", "[windows][wsa][rio]")
{
	auto const socket = posix::create_socket(
		AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP,
		io_flags::create_non_blocking | io_flags::create_registered_io).value().socket;

	auto const completion_port = create_completion_port(1).value();
	OVERLAPPED rio_overlapped;

	auto const cq = rio_create_completion_queue(
		10,
		completion_port.get(),
		nullptr,
		rio_overlapped).value();

	auto const rq = rio_create_request_queue(
		socket.get(),
		10,
		10,
		10,
		10,
		cq.get(),
		cq.get(),
		nullptr);

	WSA_FLAG_REGISTERED_IO;
	auto const rio = wsa_get_rio(socket.get()).value();
	int x = 0;

	WSAIoctl;
	SIO_ADDRESS_LIST_CHANGE
}
#endif
