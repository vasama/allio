#include <allio/impl/win32/afd.hpp>

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/posix/socket.hpp>
#include <allio/test/spawn.hpp>

#include <vsm/out_resource.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

static posix::socket_address make_addr()
{
	return posix::socket_address::make(
		ipv4_endpoint(ipv4_address::localhost, 51234)).value();
}

[[maybe_unused]]
static test::future<posix::unique_socket> spawn_connect(posix::socket_address const& addr)
{
	auto socket = posix::create_socket(
		AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP,
		io_flags::none).value().socket;

	return test::spawn([addr, socket = vsm_move(socket)]() mutable
	{
		posix::socket_connect(
			socket.get(),
			addr,
			deadline::never()).value();

		return vsm_move(socket);
	});
}

[[maybe_unused]]
static test::future<posix::unique_socket> spawn_accept(posix::socket_address const& addr)
{
	auto socket = posix::create_socket(
		AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP,
		io_flags::none).value().socket;

	posix::socket_listen(
		socket.get(),
		addr,
		/* backlog: */ 0).value();

	return test::spawn([socket = vsm_move(socket)]()
	{
		posix::socket_address peer_addr;
		return posix::socket_accept(
			socket.get(),
			peer_addr,
			deadline::never(),
			io_flags::none).value().socket;
	});
}

[[maybe_unused]]
static std::pair<posix::unique_socket, posix::unique_socket> make_socket_pair()
{
	auto const addr = make_addr();

	auto accept = spawn_accept(addr);
	auto connect = spawn_connect(addr);

	return { accept.get(), connect.get() };
}


static unique_handle afd_create_socket()
{
	UNICODE_STRING transport_name = {};

	unique_handle socket;
	NTSTATUS const status = AfdCreateSocket(
		vsm::out_resource(socket),
		/* EndpointFlags: */ 0,
		/* GroupID: */ NULL,
		static_cast<ULONG>(AF_INET),
		static_cast<ULONG>(SOCK_STREAM),
		static_cast<ULONG>(IPPROTO_TCP),
		FILE_SYNCHRONOUS_IO_NONALERT,
		&transport_name);

	REQUIRE(NT_SUCCESS(status));
	REQUIRE(status != STATUS_PENDING);

	return socket;
}

static void afd_bind(HANDLE const socket, ULONG const share_type, posix::socket_address const& addr)
{
	NTSTATUS const status = AfdBind(
		socket,
		share_type,
		&addr.addr,
		static_cast<ULONG>(addr.size));

	REQUIRE(NT_SUCCESS(status));
}

static void afd_listen(HANDLE const socket, ULONG const backlog)
{
	NTSTATUS const status = AfdListen(
		socket,
		backlog);

	REQUIRE(NT_SUCCESS(status));
}

static ULONG afd_select(HANDLE const socket, ULONG const events)
{
	AFD_SELECT_INFO_1 info;
	AfdInitializeSelect1(
		&info,
		socket,
		events,
		/* Exclusive: */ FALSE,
		/* Timeout: */ nullptr);

	IO_STATUS_BLOCK io_status_block;

	NTSTATUS const status = AfdSelect(
		socket,
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		&io_status_block,
		&info.Info);

	REQUIRE(NT_SUCCESS(status));
	REQUIRE(status != STATUS_PENDING);

	REQUIRE(NT_SUCCESS(info.Handle.Status));
	REQUIRE(info.Handle.Status != STATUS_PENDING);

	return info.Handle.Events;
}


TEST_CASE("AFD Socket connect", "[windows][afd]")
{
	auto const addr = make_addr();
	auto const accept = spawn_accept(addr);


	auto const socket = afd_create_socket();

	// Bind
	{
		posix::socket_address bind_addr = addr;
		bind_addr.ipv4.sin_addr = {};
		bind_addr.ipv4.sin_port = {};

		afd_bind(
			socket.get(),
			AFD_SHARE_WILDCARD,
			bind_addr);
	}

	// Connect
	{
		IO_STATUS_BLOCK io_status_block;

		NTSTATUS const status = AfdConnect(
			socket.get(),
			/* Event: */ NULL,
			/* ApcRoutine: */ nullptr,
			/* ApcContext: */ nullptr,
			&io_status_block,
			&addr.addr,
			static_cast<ULONG>(addr.size));

		REQUIRE(NT_SUCCESS(status));
		REQUIRE(status != STATUS_PENDING);
	}
}

TEST_CASE("AFD Socket accept", "[windows][afd]")
{
	auto const addr = make_addr();
	auto const socket = afd_create_socket();

	afd_bind(
		socket.get(),
		AFD_SHARE_UNIQUE,
		addr);

	afd_listen(
		socket.get(),
		/* backlog: */ 1);

	auto const connect = spawn_connect(addr);

	// Accept
	{
		IO_STATUS_BLOCK io_status_block;

		struct accept_result_type
		{
			AFD_RECEIVED_ACCEPT_DATA data;
			posix::socket_address_union addr;
		};
		accept_result_type accept;

		{
			NTSTATUS const status = AfdWaitForAccept(
				socket.get(),
				/* Event: */ NULL,
				/* ApcRoutine: */ nullptr,
				/* ApcContext: */ nullptr,
				&io_status_block,
				&accept.data,
				sizeof(accept));

			REQUIRE(NT_SUCCESS(status));
		}

		auto const accept_socket = afd_create_socket();
		{
			NTSTATUS const status = AfdAccept(
				socket.get(),
				/* Event: */ NULL,
				/* ApcRoutine: */ nullptr,
				/* ApcContext: */ nullptr,
				&io_status_block,
				accept_socket.get(),
				accept.data.SequenceNumber);

			REQUIRE(NT_SUCCESS(status));
		}
	}
}

TEST_CASE("AFD Listen socket polling", "[windows][afd]")
{
	auto const addr = make_addr();

	auto const socket = afd_create_socket();
	afd_bind(socket.get(), AFD_SHARE_UNIQUE, addr);
	afd_listen(socket.get(), 1);

	auto connect = spawn_connect(addr);

	auto const events = afd_select(
		socket.get(),
		AFD_EVENT_ACCEPT);

	REQUIRE(events == AFD_EVENT_ACCEPT);
}

TEST_CASE("AFD Stream socket polling", "[windows][afd]")
{
	auto const [s1, s2] = make_socket_pair();

	ULONG expected_events = AFD_EVENT_SEND;

	if (GENERATE(0, 1))
	{
		write_buffer const buffers[] =
		{
			as_write_buffer(std::as_bytes(std::span<char const>("data")))
		};

		(void)posix::socket_gather_write(
			s2.get(),
			buffers).value();

		expected_events |= AFD_EVENT_RECEIVE;
	}

	auto const events = afd_select(
		(HANDLE)s1.get(),
		AFD_EVENT_RECEIVE | AFD_EVENT_SEND);

	REQUIRE(events == expected_events);
}
