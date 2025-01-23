#pragma once

#include <allio/win32/detail/wsa.hpp>

#include <allio/detail/byte_io_buffers.hpp>
#include <allio/impl/posix/socket.hpp>

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

template<std::unsigned_integral SizeT>
vsm::result<void> check_wsa_buffers_size(detail::new_io_buffers_base const& buffers)
{
	if (buffers.was_truncated() ||
		buffers.get_buffers_size() > std::numeric_limits<SizeT>::max())
	{
		//TODO: Return a more specific error code.
		return vsm::unexpected(allio_error(error::invalid_argument));
	}

	return {};
}

template<vsm::any_cv_of<std::byte> T, size_t StorageSize = /*TODO:*/ 0>
vsm::result<detail::new_io_buffers_view> get_wsa_buffers(
	detail::_wsa_buffers_storage<StorageSize>& storage,
	detail::new_io_buffers<T> const buffers)
{
	return get_io_buffers(
		storage,
		buffers,
		detail::new_io_buffer_layout::size_data | detail::new_io_buffer_layout::size_le32);
}


struct wsa_accept_address_buffer
{
	class socket_address_buffer : public posix::socket_address_union
	{
		// AcceptEx requires an extra 16 bytes.
		[[maybe_unused]] std::byte m_dummy_buffer[16];
	};

	socket_address_buffer local;
	socket_address_buffer remote;
};

DWORD wsa_accept_ex(
	SOCKET listen_socket,
	SOCKET accept_socket,
	wsa_accept_address_buffer& address,
	OVERLAPPED& overlapped);

DWORD wsa_connect_ex(
	SOCKET socket,
	posix::socket_address const& addr,
	OVERLAPPED& overlapped);

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
