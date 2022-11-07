#pragma once

#include <allio/directory_handle.hpp>
#include <allio/file_handle.hpp>
#include <allio/implementation.hpp>
#include <allio/path_handle.hpp>
#include <allio/process_handle.hpp>
#include <allio/socket_handle.hpp>

namespace allio {

#define allio_ASYNC_HANDLE_TYPES(X, ...) \
/*	X(directory_handle                  __VA_OPT__(, __VA_ARGS__)) */\
/*	X(directory_stream_handle           __VA_OPT__(, __VA_ARGS__)) */\
	X(file_handle                       __VA_OPT__(, __VA_ARGS__)) \
/*	X(path_handle                       __VA_OPT__(, __VA_ARGS__)) */\
	X(process_handle                    __VA_OPT__(, __VA_ARGS__)) \
	X(stream_socket_handle              __VA_OPT__(, __VA_ARGS__)) \
/*	X(packet_socket_handle              __VA_OPT__(, __VA_ARGS__)) */\
	X(listen_socket_handle              __VA_OPT__(, __VA_ARGS__)) \


namespace detail {

template<typename, typename... Handles>
using make_async_handle_types = type_list<Handles...>;

} // namespace detail

using async_handle_types = detail::make_async_handle_types<
	void

#define allio_X(handle_type) \
	, handle_type

	allio_ASYNC_HANDLE_TYPES(allio_X)
#undef allio_X
>;


#define allio_detail_EXTERN_ASYNC_HANDLE_MULTIPLEXER_RELATIONS(H, M) \
	extern allio_HANDLE_MULTIPLEXER_IMPLEMENTATION(M, H);

#define allio_EXTERN_ASYNC_HANDLE_MULTIPLEXER_RELATIONS(M) \
	allio_ASYNC_HANDLE_TYPES(allio_detail_EXTERN_ASYNC_HANDLE_MULTIPLEXER_RELATIONS, M)

} // namespace allio
