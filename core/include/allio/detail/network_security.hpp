#pragma once

#include <allio/detail/filesystem.hpp>
#include <allio/detail/parameters.hpp>

#include <vsm/assert.h>
#include <vsm/flags.hpp>
#include <vsm/standard.hpp>
#include <vsm/utility.hpp>

#include <optional>
#include <span>
#include <string_view>

#include <cstdint>

namespace allio::detail {

enum class tls_options : uint_least8_t
{
	use_system_certificates            = 1 << 0,
};
vsm_flag_enum(tls_options);

enum class tls_version : uint_least8_t
{
	default_value,
	ssl_1,
	ssl_2,
	ssl_3,
	tls_1_0,
	tls_1_1,
	tls_1_2,
	tls_1_3,
};

enum class tls_verification : uint_least8_t
{
	default_value,
	none,
	optional,
	required,
};

enum class tls_secret_kind : uint_least8_t
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
	std::string_view m_mime_type;

public:
	tls_secret()
		: m_none{}
		, m_kind(tls_secret_kind::none)
	{
	}

	template<std::convertible_to<path_type> Path>
	tls_secret(Path const& path, std::string_view const mime_type = {})
		: m_path(path)
		, m_kind(tls_secret_kind::path)
		, m_mime_type(mime_type)
	{
	}

	explicit tls_secret(data_type const data, std::string_view const mime_type = {})
		: m_data(data)
		, m_kind(tls_secret_kind::data)
		, m_mime_type(mime_type)
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

	[[nodiscard]] std::string_view mime_type() const
	{
		return m_mime_type;
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_kind != tls_secret_kind::none;
	}
};


struct tls_min_version_t : explicit_argument<tls_min_version_t, tls_version> {};
inline constexpr explicit_parameter<tls_min_version_t> tls_min_version = {};

struct tls_certificate_t : explicit_argument<tls_certificate_t, tls_secret> {};
inline constexpr explicit_parameter<tls_certificate_t> tls_certificate = {};

struct tls_private_key_t : explicit_argument<tls_private_key_t, tls_secret> {};
inline constexpr explicit_parameter<tls_private_key_t> tls_private_key = {};


struct security_context_parameters
{
	tls_options options = tls_options::use_system_certificates;
	tls_version min_version = tls_version::default_value;
	tls_verification verification = tls_verification::default_value;
	tls_secret certificate;
	tls_secret private_key;

	void set_argument(tls_min_version_t const value)
	{
		min_version = value.value;
	}

	void set_argument(tls_verification const value)
	{
		verification = value;
	}

	void set_argument(tls_certificate_t const& value)
	{
		certificate = value.value;
	}

	void set_argument(tls_private_key_t const& value)
	{
		private_key = value.value;
	}

	[[deprecated]] friend void tag_invoke(
		set_argument_t,
		security_context_parameters& args,
		tls_min_version_t const value)
	{
		args.min_version = value.value;
	}

	[[deprecated]] friend void tag_invoke(
		set_argument_t,
		security_context_parameters& args,
		tls_certificate_t const value)
	{
		args.certificate = value.value;
	}

	[[deprecated]] friend void tag_invoke(
		set_argument_t,
		security_context_parameters& args,
		tls_private_key_t const value)
	{
		args.private_key = value.value;
	}

	[[deprecated]] friend void tag_invoke(
		set_argument_t,
		security_context_parameters& args,
		detail::tls_verification const value)
	{
		args.verification = value;
	}
};


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
