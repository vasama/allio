#include <allio/secure_socket_handle.hpp>

#include <schannel.h>

using namespace allio;

namespace {




struct schannel_stream_socket_handle : secure_stream_socket_handle
{

};

struct schannel_listen_socket_handle : secure_listen_socket_handle
{

};

} // namespace
