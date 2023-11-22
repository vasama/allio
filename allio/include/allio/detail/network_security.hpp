#pragma once

#include <allio/detail/parameters.hpp>

#include <vsm/assert.h>
#include <vsm/standard.hpp>
#include <vsm/utility.hpp>

#include <optional>
#include <span>
#include <string_view>

#include <cstdint>

namespace allio::detail {

enum class tls_version : unsigned char
{
	ssl_1,
	ssl_2,
	ssl_3,
	tls_1_0,
	tls_1_1,
	tls_1_2,
	tls_1_3,
};

enum class tls_verification : unsigned char
{
	none,
	optional,
	required,
};

class tls_secret
{
};


struct tls_min_version_t
{
	std::optional<tls_version> tls_min_version;
};
inline constexpr explicit_parameter<tls_min_version_t> tls_min_version = {};

struct tls_certificate_t
{
	tls_secret tls_certificate;
};
inline constexpr explicit_parameter<tls_certificate_t> tls_certificate = {};

struct tls_private_key_t
{
	tls_secret tls_private_key;
};
inline constexpr explicit_parameter<tls_private_key_t> tls_private_key = {};

struct _tls_verification_t
{
	std::optional<detail::tls_verification> tls_verification;
};
using tls_verification_t = implicit_parameter<_tls_verification_t>;


using security_context_parameters = parameters_t
<
	tls_min_version_t,
	tls_certificate_t,
	tls_private_key_t,
	tls_verification_t
>;


struct no_security_t {};
inline constexpr no_security_t no_security = {};

template<typename SecurityContext>
struct basic_socket_security_context_t
{
	SecurityContext const* security_context = nullptr;

	friend void tag_invoke(
		set_argument_t,
		basic_socket_security_context_t& args,
		SecurityContext const& security_context)
	{
		args.security_context = &security_context;
	}
};

template<>
struct basic_socket_security_context_t<void>
{
	friend void tag_invoke(
		set_argument_t,
		basic_socket_security_context_t&,
		no_security_t)
	{
	}
};

} // namespace allio::detail
