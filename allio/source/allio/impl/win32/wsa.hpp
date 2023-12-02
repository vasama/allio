#pragma once

#include <allio/win32/detail/wsa.hpp>

#include <allio/impl/posix/socket.hpp>

#include <vsm/lazy.hpp>
#include <vsm/result.hpp>

#include <algorithm>
#include <limits>
#include <memory>

#include <MSWSock.h>

namespace allio::win32 {

vsm::result<void> wsa_init();


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


template<vsm::any_cv_of<std::byte> T>
inline vsm::result<void> transform_wsa_buffers(basic_buffers<T> const buffers, WSABUF* const wsa_buffers)
{
	bool size_out_of_range = false;

	std::transform(
		buffers.begin(),
		buffers.end(),
		wsa_buffers,
		[&](basic_buffer<T> const buffer) -> WSABUF
		{
			if (buffer.size() > std::numeric_limits<ULONG>::max())
			{
				size_out_of_range = true;
			}

			return WSABUF
			{
				.len = static_cast<ULONG>(buffer.size()),
				.buf = reinterpret_cast<CHAR*>(const_cast<std::byte*>(buffer.data())),
			};
		}
	);

	if (size_out_of_range)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	return {};
}

template<vsm::any_cv_of<std::byte> T, size_t StorageSize>
vsm::result<std::span<WSABUF>> make_wsa_buffers(
	detail::_wsa_buffers_storage<StorageSize>& storage,
	basic_buffers<T> const buffers)
{
	vsm_try(wsa_buffers, storage.reserve(buffers.size()));
	vsm_try_void(transform_wsa_buffers(buffers, wsa_buffers));
	return std::span<WSABUF>(wsa_buffers, buffers.size());
}


struct wsa_accept_address_buffer
{
	class socket_address_buffer : public posix::socket_address_union
	{
		// AcceptEx requires an extra 16 bytes.
		std::byte m_dummy_buffer[16];
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
	void vsm_static_operator_invoke(RIO_CQ const cq)
	{
		rio_close_completion_queue(cq);
	}
};
using unique_rio_cq = std::unique_ptr<RIO_CQ_t, rio_cq_deleter>;

struct rio_buffer_deleter
{
	void vsm_static_operator_invoke(RIO_BUFFERID const buffer_id)
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
