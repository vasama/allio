#pragma once

#include <allio/detail/filesystem.hpp>
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

enum class tls_secret_kind : unsigned char
{
	none,
	data,
	path,
};

class tls_secret
{
	using data_type = std::span<std::byte const>;
	using path_type = fs_path;

	union
	{
		struct {} m_none;
		data_type m_data;
		path_type m_path;
	};
	tls_secret_kind m_kind;

public:
	tls_secret()
		: m_none{}
		, m_kind(tls_secret_kind::none)
	{
	}

	template<std::convertible_to<path_type> Path>
	tls_secret(Path const& path)
		: m_path(path)
		, m_kind(tls_secret_kind::path)
	{
	}

	explicit tls_secret(data_type const data)
		: m_data(data)
		, m_kind(tls_secret_kind::data)
	{
	}

	[[nodiscard]] tls_secret_kind kind() const
	{
		return m_kind;
	}

	[[nodiscard]] bool is_data() const
	{
		return m_kind == tls_secret_kind::data;
	}

	[[nodiscard]] data_type data() const
	{
		vsm_assert(m_kind == tls_secret_kind::data);
		return m_data;
	}

	[[nodiscard]] bool is_path() const
	{
		return m_kind == tls_secret_kind::path;
	}

	[[nodiscard]] path_type const& path() const
	{
		vsm_assert(m_kind == tls_secret_kind::path);
		return m_path;
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_kind != tls_secret_kind::none;
	}
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

struct tls_verification_t
{
	std::optional<detail::tls_verification> tls_verification;

	friend void tag_invoke(
		set_argument_t,
		tls_verification_t& args,
		detail::tls_verification const verification)
	{
		args.tls_verification = verification;
	}
};


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
struct basic_security_context_t
{
	SecurityContext const* security_context = nullptr;

	friend void tag_invoke(
		set_argument_t,
		basic_security_context_t& args,
		SecurityContext const& security_context)
	{
		args.security_context = &security_context;
	}
};

template<>
struct basic_security_context_t<void>
{
	friend void tag_invoke(
		set_argument_t,
		basic_security_context_t&,
		no_security_t)
	{
	}
};

} // namespace allio::detail
