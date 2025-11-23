#pragma once

#include <allio/detail/network_security.hpp>

namespace allio::detail {

class secure_socket_provider
{
public:
	[[nodiscard]] virtual std::string_view name() const = 0;
	[[nodiscard]] virtual std::string_view version() const = 0;

	[[nodiscard]] virtual tls_version min_tls_version() const = 0;
	[[nodiscard]] virtual tls_version max_tls_version() const = 0;

	[[nodiscard]] virtual socket_handle make_stream_socket() const = 0;
	[[nodiscard]] virtual listen_socket_handle make_listen_socket() const = 0;

	[[nodiscard]] virtual vsm::result<socket_handle> wrap_stream_socket(
		raw_socket_handle h) const = 0;

	[[nodiscard]] virtual vsm::result<listen_socket_handle> wrap_listen_socket(
		raw_listen_socket_handle h) const = 0;

protected:
	secure_socket_provider() = default;
	secure_socket_provider(secure_socket_provider const&) = default;
	secure_socket_provider& operator=(secure_socket_provider const&) = default;
};

[[nodiscard]] vsm::result<void> register_socket_provider(secure_socket_provider const& provider);

[[nodiscard]] secure_socket_provider const& raw_socket_provider();
[[nodiscard]] secure_socket_provider const& tls_socket_provider();

} // namespace allio::detail
