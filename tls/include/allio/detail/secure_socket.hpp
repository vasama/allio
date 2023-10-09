#pragma once

#include <vsm/linear.hpp>

#include <span>
#include <string_view>

namespace allio::detail {

class secure_socket_provider;

class secure_socket_object
{
protected:
	allio_detail_default_lifetime(secure_socket_object);
};

class secure_socket_interface
{
	std::string_view m_name;

public:
	virtual secure_socket_object* create_stream_socket();
	virtual secure_socket_object* create_listen_socket();

protected:
	explicit secure_socket_interface(std::string_view const name)
		: m_name(name)
	{
	}

private:
	friend secure_socket_provider;
};


class secure_socket_provider
{
	vsm::linear<secure_socket_interface*> m_interface;

public:
	[[nodiscard]] std::string_view name() const
	{
		return m_interface->m_name;
	}

	static std::span<secure_socket_provider const> get_providers();
};

} // namespace allio::detail
