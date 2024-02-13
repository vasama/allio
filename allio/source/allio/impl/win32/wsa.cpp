#include <allio/impl/win32/wsa.hpp>

#include <vsm/atomic.hpp>
#include <vsm/defer.hpp>
#include <vsm/numeric.hpp>
#include <vsm/unique_resource.hpp>

#pragma comment(lib, "ws2_32.lib")

using namespace allio;
using namespace allio::posix;
using namespace allio::win32;

static_assert(
	offsetof(wsa_accept_address_buffer, local) == 0 &&
	offsetof(wsa_accept_address_buffer, remote) == sizeof(wsa_accept_address_buffer::local),
	"AcceptEx writes the local and remote addresses into the same buffer, one after the other.");


#define allio_msw_functions(X) \
	X(AcceptEx,                         FALSE,              WSAID_ACCEPTEX) \
	X(ConnectEx,                        FALSE,              WSAID_CONNECTEX) \
	X(WSASendMsg,                       SOCKET_ERROR,       WSAID_WSASENDMSG) \
	X(WSARecvMsg,                       SOCKET_ERROR,       WSAID_WSARECVMSG) \

#define allio_rio_functions(X) \
	X(RIOSend,                          FALSE) \
	X(RIOSendEx,                        FALSE) \
	X(RIOReceive,                       FALSE) \
	X(RIOReceiveEx,                     FALSE) \
	X(RIOCloseCompletionQueue,          0) \
	X(RIOCreateCompletionQueue,         RIO_INVALID_CQ) \
	X(RIOCreateRequestQueue,            RIO_INVALID_RQ) \
	X(RIODequeueCompletion,             0) \
	X(RIODeregisterBuffer,              0) \
	X(RIONotify,                        WSAEOPNOTSUPP) \
	X(RIORegisterBuffer,                RIO_INVALID_BUFFERID) \
	X(RIOResizeCompletionQueue,         FALSE) \
	X(RIOResizeRequestQueue,            FALSE) \

namespace {

template<typename T>
static bool get_extension(SOCKET const socket, DWORD const io_control_code, GUID extension, T& out)
{
	if (socket == INVALID_SOCKET)
	{
		return false;
	}

	DWORD bytes_returned;
	if (::WSAIoctl(
		socket,
		io_control_code,
		&extension,
		sizeof(extension),
		&out,
		sizeof(out),
		&bytes_returned,
		/* lpOverlapped: */ nullptr,
		/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
	{
		return false;
	}
	vsm_assert(bytes_returned == sizeof(out));

	return true;
}

template<typename P>
struct wsa_unsupported;

template<typename R, typename... Ps>
struct wsa_unsupported<R(WINAPI*)(Ps...)>
{
	template<auto ReturnValue>
	static R WINAPI function(Ps...)
	{
		WSASetLastError(WSAEOPNOTSUPP);
		return ReturnValue;
	}
};

template<typename... Ps>
struct wsa_unsupported<void(WINAPI*)(Ps...)>
{
	template<auto ReturnValue>
	static void WINAPI function(Ps...)
	{
	}
};


static void wsa_init_lazy();

template<typename P>
struct wsa_function;

template<typename R, typename... Ps>
struct wsa_function<R(WINAPI*)(Ps...)>
{
	using Pointer = R(WINAPI*)(Ps...);

	template<Pointer const& Function>
	static R WINAPI function(Ps const... args)
	{
		(void)wsa_init_lazy();
		return Function(args...);
	}
};


template<typename T>
static void set_function(T& object, T const value)
{
	vsm::atomic_ref<T>(object).store(value, std::memory_order_relaxed);
}

template<auto ReturnValue, typename R, typename... Ps>
static void msw_init_function(SOCKET const socket, R(WINAPI*& function)(Ps...), GUID const& extension)
{
	R(WINAPI* new_function)(Ps...);

	if (!get_extension(
		socket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		extension,
		new_function))
	{
		new_function = wsa_unsupported<R(WINAPI*)(Ps...)>::template function<ReturnValue>;
	}

	set_function(function, new_function);
}

static void msw_init(SOCKET const socket)
{
#define allio_x_entry(f, default_value, id) \
	msw_init_function<default_value>(socket, win32::f, id);

	allio_msw_functions(allio_x_entry)
#undef allio_x_entry
}

static void rio_init(SOCKET const socket)
{
	RIO_EXTENSION_FUNCTION_TABLE table = {};

	bool const r = get_extension(
		socket,
		SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
		WSAID_MULTIPLE_RIO,
		table);
	vsm_assert(!r || table.cbSize == sizeof(table));

	#define allio_x_entry(f, default_value) \
		set_function(win32::f, r \
			? table.f \
			: wsa_unsupported<decltype(win32::f)>::function<default_value>); \

	allio_rio_functions(allio_x_entry)
	#undef allio_x_entry
}


#if 0
class wsa_startup_t
{
	socket_error m_error;

public:
	wsa_startup_t()
	{
		WSADATA data;
		int const e = WSAStartup(MAKEWORD(2, 2), &data);
		m_error = e == 0
			? socket_error::none
			: static_cast<socket_error>(e);
	}

	wsa_startup_t(wsa_startup_t&& other)
		: m_error(other.m_error)
	{
		other.m_error = static_cast<socket_error>(-1);
	}

	wsa_startup_t& operator=(wsa_startup_t&& other) = delete;

	~wsa_startup_t()
	{
		if (m_error == socket_error::none)
		{
			vsm_verify(WSACleanup() == 0);
		}
	}

	explicit operator bool() const
	{
		return m_error == socket_error::none;
	}

	socket_error get_error() const
	{
		return m_error;
	}
};
#endif

struct wsa_deleter
{
	void vsm_static_operator_invoke(bool)
	{
		vsm_verify(WSACleanup() == ERROR_SUCCESS);
	}
};
using unique_wsa_startup = vsm::unique_resource<bool, wsa_deleter, false>;

static unique_wsa_startup wsa_startup()
{
	WSADATA data;
	if (WSAStartup(MAKEWORD(2, 2), &data) == ERROR_SUCCESS)
	{
		return unique_wsa_startup(true);
	}
	return {};
}

static unique_wsa_startup wsa_init()
{
	unique_wsa_startup startup = wsa_startup();

	// Initialize standard WSA functions.
	{
		// Get the functions directly from the DLL to avoid the indirection.
		HMODULE const ws2_32 = GetModuleHandleW(L"WS2_32.DLL");

		auto const set_wsa_function = [&]<typename F>(F& a_f, F const w_f, char const* const name)
		{
			FARPROC const direct = ws2_32 == nullptr
				? nullptr
				: GetProcAddress(ws2_32, name);

			F const function = direct == nullptr
				? w_f
				: reinterpret_cast<F>(direct);

			set_function(a_f, function);
		};

		#define allio_x_entry(f) \
			set_wsa_function(win32::f, &::f, vsm_pp_str(f));

		allio_wsa_functions(allio_x_entry)
		#undef allio_x_entry
	}

	SOCKET socket = INVALID_SOCKET;

	vsm_defer
	{
		if (socket != INVALID_SOCKET)
		{
			vsm_verify(closesocket(socket) == 0);
		}
	};

	if (startup)
	{
		socket = ::WSASocketW(
			AF_INET,
			SOCK_STREAM,
			IPPROTO_TCP,
			/* lpProtocolInfo: */ nullptr,
			/* group: */ NULL,
			/* dwFlags: */ 0);
	}

	msw_init(socket);
	rio_init(socket);

	return startup;
}

static void wsa_init_lazy()
{
	static unique_wsa_startup const startup = wsa_init();
}

} // namespace

#define allio_x_entry(f, ...) \
	decltype(win32::f) win32::f = \
		wsa_function<decltype(win32::f)>::function<win32::f>;

allio_wsa_functions(allio_x_entry)
allio_msw_functions(allio_x_entry)
allio_rio_functions(allio_x_entry)
#undef allio_x_entry


DWORD win32::wsa_accept_ex(
	SOCKET const listen_socket,
	SOCKET const accept_socket,
	wsa_accept_address_buffer& address,
	OVERLAPPED& overlapped)
{
	DWORD transferred = static_cast<DWORD>(-1);
	if (!win32::AcceptEx(
		listen_socket,
		accept_socket,
		/* lpOutputBuffer: */ &address,
		/* dwReceiveDataLength: */ 0,
		sizeof(address.local),
		sizeof(address.remote),
		&transferred,
		&overlapped))
	{
		return WSAGetLastError();
	}
	vsm_assert(transferred == 0);
	return 0;
}

DWORD win32::wsa_connect_ex(
	SOCKET const socket,
	socket_address const& addr,
	OVERLAPPED& overlapped)
{
	DWORD transferred = static_cast<DWORD>(-1);
	if (!win32::ConnectEx(
		socket,
		&addr.addr,
		addr.size,
		/* lpSendBuffer: */ nullptr,
		/* dwSendDataLength: */ 0,
		&transferred,
		&overlapped))
	{
		return WSAGetLastError();
	}
	vsm_assert(transferred == 0);
	return 0;
}


static DWORD wsa_send_msg(
	SOCKET const socket,
	LPWSAMSG const message,
	LPDWORD const transferred,
	LPOVERLAPPED const overlapped)
{
	if (win32::WSASendMsg(
		socket,
		message,
		/* dwFlags: */ 0,
		transferred,
		overlapped,
		/* lpCompletionRoutine: */ nullptr) == socket_error_value)
	{
		return WSAGetLastError();
	}
	return 0;
}

static DWORD wsa_recv_msg(
	SOCKET const socket,
	LPWSAMSG const message,
	LPDWORD const transferred,
	LPOVERLAPPED const overlapped)
{
	if (win32::WSARecvMsg(
		socket,
		message,
		transferred,
		overlapped,
		/* lpCompletionRoutine: */ nullptr) == socket_error_value)
	{
		return WSAGetLastError();
	}
	return 0;
}


DWORD win32::wsa_send_msg(
	SOCKET const socket,
	LPWSAMSG const message,
	LPDWORD const transferred)
{
	return wsa_send_msg(socket, message, transferred, nullptr);
}

DWORD win32::wsa_send_msg(
	SOCKET const socket,
	LPWSAMSG const message,
	LPOVERLAPPED const overlapped)
{
	return wsa_send_msg(socket, message, nullptr, overlapped);
}

DWORD win32::wsa_recv_msg(
	SOCKET const socket,
	LPWSAMSG const message,
	LPDWORD const transferred)
{
	return wsa_recv_msg(socket, message, transferred, nullptr);
}

DWORD win32::wsa_recv_msg(
	SOCKET const socket,
	LPWSAMSG const message,
	LPOVERLAPPED const overlapped)
{
	return wsa_recv_msg(socket, message, nullptr, overlapped);
}


void win32::rio_close_completion_queue(RIO_CQ const cq)
{
	win32::RIOCloseCompletionQueue(cq);
}

void win32::rio_deregister_buffer(RIO_BUFFERID const buffer_id)
{
	win32::RIODeregisterBuffer(buffer_id);
}

vsm::result<unique_rio_cq> win32::rio_create_completion_queue(
	size_t const queue_size,
	HANDLE const completion_port,
	void* const completion_key,
	OVERLAPPED& overlapped)
{
	vsm_try(queue_size_dword, vsm::try_truncate<DWORD>(queue_size, error::invalid_argument));

	RIO_NOTIFICATION_COMPLETION completion;
	RIO_NOTIFICATION_COMPLETION* p_completion = nullptr;

	if (completion_port != NULL)
	{
		completion.Type = RIO_IOCP_COMPLETION;
		completion.Iocp.IocpHandle = completion_port;
		completion.Iocp.CompletionKey = completion_key;
		completion.Iocp.Overlapped = &overlapped;
		p_completion = &completion;
	}

	RIO_CQ const cq = win32::RIOCreateCompletionQueue(
		queue_size_dword,
		p_completion);

	if (cq == RIO_INVALID_CQ)
	{
		return vsm::unexpected(posix::get_last_socket_error());
	}

	return vsm_lazy(unique_rio_cq(cq));
}

vsm::result<RIO_RQ> win32::rio_create_request_queue(
	SOCKET const socket,
	size_t const max_outstanding_receive,
	size_t const max_receive_data_buffers,
	size_t const max_outstanding_send,
	size_t const max_send_data_buffers,
	RIO_CQ const receive_cq,
	RIO_CQ const send_cq,
	void* const socket_context)
{
	vsm_try(max_recv_outs, vsm::try_truncate<ULONG>(max_outstanding_receive, error::invalid_argument));
	vsm_try(max_recv_bufs, vsm::try_truncate<ULONG>(max_receive_data_buffers, error::invalid_argument));
	vsm_try(max_send_outs, vsm::try_truncate<ULONG>(max_outstanding_send, error::invalid_argument));
	vsm_try(max_send_bufs, vsm::try_truncate<ULONG>(max_send_data_buffers, error::invalid_argument));

	RIO_RQ const rq = win32::RIOCreateRequestQueue(
		socket,
		max_recv_outs,
		max_recv_bufs,
		max_send_outs,
		max_send_bufs,
		receive_cq,
		send_cq,
		socket_context);

	if (rq == RIO_INVALID_RQ)
	{
		return vsm::unexpected(posix::get_last_socket_error());
	}

	return rq;
}

vsm::result<unique_rio_buffer> win32::rio_register_buffer(
	std::span<std::byte> const buffer)
{
	RIO_BUFFERID const buffer_id = win32::RIORegisterBuffer(
		reinterpret_cast<PCHAR>(buffer.data()),
		vsm::saturating(buffer.size()));

	if (buffer_id == RIO_INVALID_BUFFERID)
	{
		return vsm::unexpected(posix::get_last_socket_error());
	}

	return vsm_lazy(unique_rio_buffer(buffer_id));
}

size_t win32::rio_dequeue_completion(
	RIO_CQ const cq,
	std::span<RIORESULT> const buffer)
{
	auto const size = win32::RIODequeueCompletion(
		cq,
		buffer.data(),
		vsm::saturating(buffer.size()));
	vsm_assert(size != RIO_CORRUPT_CQ);
	return size;
}
