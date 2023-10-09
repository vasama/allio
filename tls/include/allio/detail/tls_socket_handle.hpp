#pragma once

#include <allio/detail/secure_socket.hpp>
#include <allio/socket_handle.hpp>

#include <vsm/linear.hpp>

namespace allio::detail {

class _secure_stream_socket_handle : public handle
{
protected:
	using base_type = handle;

private:
	//stream_socket_handle m_socket;
	vsm::linear<secure_socket_provider*> m_provider;
	vsm::linear<secure_socket_context*> m_context;


};

class _secure_listen_socket_handle : public handle
{
protected:
	using base_type = handle;

private:
	//listen_socket_handle m_socket;
	vsm::linear<secure_socket_provider*> m_provider;
	vsm::linear<secure_socket_context*> m_context;
};

} // namespace allio::detail
