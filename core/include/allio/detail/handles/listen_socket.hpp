#pragma once

#include <allio/detail/any_object.hpp>
#include <allio/detail/handles/listen_socket_base.hpp>

namespace allio::detail {

template<multiplexer... Multiplexers>
using basic_listen_socket_t = any_object_t<listen_socket_base_t<object_t>, Multiplexers...>;

//TODO: Think more about whether this is ok wrt. orphan customisations and whatnot.
//      Instead maybe move this to the public header.
//struct default_multiplexer;
//using listen_socket_t = basic_listen_socket_t<default_multiplexer>;

} // namespace allio::detail
