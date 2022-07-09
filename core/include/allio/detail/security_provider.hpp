#pragma once

#include <allio/detail/network_security.hpp>

namespace allio::detail {

class network_security_provider
{
public:
	virtual std::string_view name() const = 0;
	virtual std::string_view version() const = 0;
};

network_security_provider const* get_default_network_security_provider();
std::span<network_security_provider const*> get_network_security_providers();

} // namespace allio::detail
