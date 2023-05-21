#pragma once

#include <allio/directory_handle.hpp>
#include <allio/file_handle.hpp>
#include <allio/implementation.hpp>
#include <allio/path_handle.hpp>
#include <allio/process_handle.hpp>
#include <allio/socket_handle.hpp>

namespace allio {

#define allio_async_handle_types(X, ...) \
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

#define allio_x_entry(handle_type) \
	, handle_type

	allio_async_handle_types(allio_x_entry)
#undef allio_x_entry
>;


#define allio_detail_extern_async_handle_multiplexer_relations(H, M) \
	extern allio_handle_multiplexer_implementation(M, H);

#define allio_extern_async_handle_multiplexer_relations(M) \
	allio_async_handle_types(allio_detail_extern_async_handle_multiplexer_relations, M)

} // namespace allio
