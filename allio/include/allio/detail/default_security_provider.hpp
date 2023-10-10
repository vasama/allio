#pragma once

#include <allio/detail/lifetime.hpp>

#include <vsm/linear.hpp>
#include <vsm/result.hpp>

#include <string_view>

namespace allio::detail {

class secure_socket_object
{
protected:
	allio_detail_default_lifetime(secure_socket_object);
};

class secure_socket_source
{
public:
	virtual vsm::result<secure_socket_object*> create_stream_socket();
	virtual vsm::result<secure_socket_object*> create_listen_socket();
	virtual vsm::result<secure_socket_object*> create_datagram_socket();

protected:
	allio_detail_default_lifetime(secure_socket_source);
};

struct default_security_provider
{
	class socket_type
	{
		vsm::linear<secure_socket_object*> m_socket;

	public:
		
	};
};

} // namespace allio::detail
