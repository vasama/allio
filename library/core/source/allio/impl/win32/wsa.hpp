#pragma once

#include <allio/detail/byte_io_buffers.hpp>
#include <allio/impl/posix/socket.hpp>
#include <allio/impl/storage_provider.hpp>

#include <vsm/lazy.hpp>
#include <vsm/result.hpp>

#include <algorithm>
#include <limits>
#include <memory>

#include <MSWSock.h>

namespace allio::win32 {

#define allio_wsa_functions(X) \
	X(WSASocketW) \
	X(WSAIoctl) \
	X(WSAPoll) \
	X(WSAAccept) \
	X(WSASend) \
	X(WSARecv) \
	X(WSASendTo) \
	X(WSARecvFrom) \

#define allio_x_entry(f) \
	extern decltype(::f)* f;

allio_wsa_functions(allio_x_entry)
#undef allio_x_entry

extern LPFN_ACCEPTEX AcceptEx;
extern LPFN_CONNECTEX ConnectEx;
extern LPFN_WSASENDMSG WSASendMsg;
extern LPFN_WSARECVMSG WSARecvMsg;

extern LPFN_RIORECEIVE RIOReceive;
extern LPFN_RIORECEIVEEX RIOReceiveEx;
extern LPFN_RIOSEND RIOSend;
extern LPFN_RIOSENDEX RIOSendEx;
extern LPFN_RIOCLOSECOMPLETIONQUEUE RIOCloseCompletionQueue;
extern LPFN_RIOCREATECOMPLETIONQUEUE RIOCreateCompletionQueue;
extern LPFN_RIOCREATEREQUESTQUEUE RIOCreateRequestQueue;
extern LPFN_RIODEQUEUECOMPLETION RIODequeueCompletion;
extern LPFN_RIODEREGISTERBUFFER RIODeregisterBuffer;
extern LPFN_RIONOTIFY RIONotify;
extern LPFN_RIOREGISTERBUFFER RIORegisterBuffer;
extern LPFN_RIORESIZECOMPLETIONQUEUE RIOResizeCompletionQueue;
extern LPFN_RIORESIZEREQUESTQUEUE RIOResizeRequestQueue;


#if 0
using detail::wsa_address_storage;

template<typename AddressBuffer, size_t Size>
AddressBuffer& new_wsa_address_buffer(wsa_address_storage<Size>& storage)
{
	static_assert(sizeof(wsa_address_storage<Size>) >= sizeof(AddressBuffer));
	static_assert(alignof(wsa_address_storage<Size>) >= alignof(AddressBuffer));
	return *new (storage.storage) AddressBuffer;
}

template<typename AddressBuffer, size_t Size>
AddressBuffer& get_wsa_address_buffer(wsa_address_storage<Size>& storage)
{
	return *std::launder(reinterpret_cast<AddressBuffer*>(storage.storage));
}
#endif


#if 0
template<vsm::any_cv_of<std::byte> T>
inline ULONG transform_wsa_buffers(basic_buffers<T> const buffers, WSABUF* const wsa_buffers)
{
	ULONG wsa_buffer_count = 0;
	ULONG max_total_size = 0;

	for (basic_buffer<T> const buffer : buffers)
	{
		bool const out_of_range = buffer.size() >= std::numeric_limits<ULONG>::max();

		ULONG const size = out_of_range
			? std::numeric_limits<ULONG>::max()
			: static_cast<ULONG>(buffer.size());

		wsa_buffers[wsa_buffer_count++] =
		{
			.len = size,
			.buf = reinterpret_cast<CHAR*>(const_cast<std::byte*>(buffer.data())),
		};

		if (out_of_range)
		{
			break;
		}

		if (wsa_buffer_count == std::numeric_limits<ULONG>::max())
		{
			break;
		}

		if (size > std::numeric_limits<ULONG>::max() - max_total_size)
		{
			break;
		}

		max_total_size += size;
	}
	return wsa_buffer_count;
}

struct wsa_buffer_span
{
	WSABUF* data;
	ULONG size;
};

template<vsm::any_cv_of<std::byte> T, size_t StorageSize>
vsm::result<wsa_buffer_span> make_wsa_buffers(
	detail::_wsa_buffers_storage<StorageSize>& storage,
	basic_buffers<T> const buffers)
{
	vsm_try(wsa_buffers, storage.reserve(buffers.size()));
	auto const count = transform_wsa_buffers(buffers, wsa_buffers);
	return wsa_buffer_span{ wsa_buffers, count };
}
#endif

inline constexpr detail::io_buffer_layout wsa_buffer_layout =
	detail::io_buffer_layout::size_data |
	detail::io_buffer_layout::size_le32;

template<std::unsigned_integral SizeT>
vsm::result<void> check_wsa_buffers_size(detail::io_buffers_base const& buffers)
{
	if (buffers.was_truncated() ||
		buffers.get_buffers_size() > std::numeric_limits<SizeT>::max())
	{
		//TODO: Return a more specific error code.
		return vsm::unexpected(allio_error(error::invalid_argument));
	}

	return {};
}

template<vsm::any_cv_of<std::byte> T>
[[nodiscard]] vsm::result<std::span<WSABUF const>> get_wsa_buffers(
	detail::io_buffers<T> const& buffers,
	storage_provider_ref const storage_provider)
{
	vsm_try(wsa_buffers, get_io_buffers(buffers, wsa_buffer_layout, storage_provider));

	return std::span(
		reinterpret_cast<WSABUF const*>(wsa_buffers.buffers_data),
		wsa_buffers.buffers_size);
}

using dynamic_wsa_buffer_storage = dynamic_storage_provider<16 * sizeof(WSABUF)>;


class wsa_accept_address_storage : public posix::socket_address_union
{
	[[maybe_unused]] std::byte m_dummy_buffer[16];

public:
	[[nodiscard]] posix::socket_address_union& address()
	{
		return *this;
	}

	[[nodiscard]] posix::socket_address_union const& address() const
	{
		return *this;
	}
};


DWORD wsa_accept_ex(
	SOCKET listen_socket,
	SOCKET accept_socket,
	PVOID address_storage,
	DWORD address_Storage_size,
	LPOVERLAPPED overlapped);

DWORD wsa_connect_ex(
	SOCKET socket,
	sockaddr const* address,
	posix::socket_address_size_type address_size,
	LPOVERLAPPED overlapped);

DWORD wsa_send_msg(
	SOCKET socket,
	LPWSAMSG message,
	LPDWORD transferred);

DWORD wsa_send_msg(
	SOCKET socket,
	LPWSAMSG message,
	LPOVERLAPPED overlapped);

DWORD wsa_recv_msg(
	SOCKET socket,
	LPWSAMSG message,
	LPDWORD transferred);

DWORD wsa_recv_msg(
	SOCKET socket,
	LPWSAMSG message,
	LPOVERLAPPED overlapped);


void rio_close_completion_queue(RIO_CQ rq);
void rio_deregister_buffer(RIO_BUFFERID buffer_id);

struct rio_cq_deleter
{
	vsm_static_operator void operator()(RIO_CQ const cq) vsm_static_operator_const
	{
		rio_close_completion_queue(cq);
	}
};
using unique_rio_cq = std::unique_ptr<RIO_CQ_t, rio_cq_deleter>;

struct rio_buffer_deleter
{
	vsm_static_operator void operator()(RIO_BUFFERID const buffer_id) vsm_static_operator_const
	{
		rio_deregister_buffer(buffer_id);
	}
};
using unique_rio_buffer = std::unique_ptr<RIO_BUFFERID_t, rio_buffer_deleter>;


vsm::result<unique_rio_cq> rio_create_completion_queue(
	size_t queue_size,
	HANDLE completion_port,
	void* completion_key,
	OVERLAPPED& overlapped);

vsm::result<void> rio_resize_completion_queue(
	RIO_CQ cq,
	size_t queue_size);

vsm::result<RIO_RQ> rio_create_request_queue(
	SOCKET socket,
	size_t max_outstanding_receive,
	size_t max_receive_data_buffers,
	size_t max_outstanding_send,
	size_t max_send_data_buffers,
	RIO_CQ receive_cq,
	RIO_CQ send_cq,
	void* socket_context);

vsm::result<void> rio_resize_request_queue(
	RIO_RQ rq,
	size_t max_outstanding_receive,
	size_t max_outstanding_send);

vsm::result<unique_rio_buffer> rio_register_buffer(
	std::span<std::byte> buffer);

size_t rio_dequeue_completion(
	RIO_CQ cq,
	std::span<RIORESULT> buffer);

} // namespace allio::win32
