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

namespace {

template<size_t Size>
class completion_storage
{
	FILE_IO_COMPLETION_INFORMATION m_completions[Size];
	size_t m_completion_count = 0;

public:
	size_t remove(
		detail::unique_handle const& completion_port,
		size_t const max_completions = static_cast<size_t>(-1))
	{
		size_t const count = remove_io_completions(
			completion_port.get(),
			std::span(m_completions).subspan(m_completion_count, max_completions),
			std::chrono::milliseconds(100)).value();
		m_completion_count += count;
		return count;
	}

	FILE_IO_COMPLETION_INFORMATION const& get(OVERLAPPED const& overlapped) const
	{
		auto const beg = m_completions;
		auto const end = m_completions + m_completion_count;
		auto const it = std::find_if(beg, end, [&](auto const& completion)
			{
				return completion.ApcContext == &overlapped;
			});
		REQUIRE(it != end);
		return *it;
	}
};

} // namespace


/* Stream sockets */

TEST_CASE("WSA supports unix stream sockets", "[windows][wsa][socket]")
{
	REQUIRE(posix::create_socket(AF_UNIX, SOCK_STREAM, 0, io_flags::none));
}

TEST_CASE("WSA asynchronous connect and accept", "[windows][wsa][socket][async]")
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


	/* Handle completions */

	completion_storage<3> completions;

	// Remove up to one completion, expecting one.
	REQUIRE(completions.remove(completion_port, 1) == 1);

	// Remove up to two completions, expecting one.
	REQUIRE(completions.remove(completion_port, 2) == 1);

	// Accept completion
	{
		auto const& completion = completions.get(accept_overlapped);
		REQUIRE(NT_SUCCESS(completion.IoStatusBlock.Status));
	}

	// Connect completion
	{
		auto const& completion = completions.get(connect_overlapped);
		REQUIRE(NT_SUCCESS(completion.IoStatusBlock.Status));
	}

#if 0
	SECTION("Asynchronous polling")
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


/* Datagram sockets */

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


	/* Receive */

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


	/* Send */

	auto const send_socket = create_socket();

	set_completion_information(
		(HANDLE)send_socket.get(),
		completion_port.get(),
		/* key_context: */ nullptr).value();

	signed char send_value = 42;

	OVERLAPPED send_overlapped = {};
	bool send_pending = false;
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
			send_pending = true;
		}
	}


	/* Handle completions */

	completion_storage<3> completions;

	// Remove up to one completion, expecting one.
	REQUIRE(completions.remove(completion_port, 1) == 1);

	// Remove up to two completions, expecting one if the send is pending.
	REQUIRE(completions.remove(completion_port, 2) == static_cast<size_t>(send_pending));

	// Receive completion
	{
		auto const& completion = completions.get(receive_overlapped);
		REQUIRE(NT_SUCCESS(completion.IoStatusBlock.Status));
		REQUIRE(completion.IoStatusBlock.Information == 1);
	}

	// Send completion
	if (send_pending)
	{
		auto const& completion = completions.get(send_overlapped);
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
