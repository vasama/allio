#pragma once

#include <allio/path_view.hpp>

#include <vsm/assert.h>
#include <vsm/int128.hpp>
#include <vsm/result.hpp>
#include <vsm/utility.hpp>

#include <bit>
#include <charconv>
#include <compare>
#include <concepts>
#include <optional>
#include <string_view>

#include <cstdint>

namespace allio {

template<std::integral T>
constexpr T network_byte_order(T const value)
{
	if constexpr (sizeof(T) == 1)
	{
		return value;
	}
	else if constexpr (std::endian::native == std::endian::little)
	{
#ifdef __cpp_lib_byteswap
		return std::byteswap(value);
#else
		/**/ if constexpr (sizeof(T) == 2)
		{
			return __builtin_bswap16(value);
		}
		else if constexpr (sizeof(T) == 4)
		{
			return __builtin_bswap32(value);
		}
		else if constexpr (sizeof(T) == 8)
		{
			return __builtin_bswap64(value);
		}
		else
		{
			static_assert(sizeof(T) == 0);
		}
#endif
	}
	else if constexpr (std::endian::native == std::endian::big)
	{
		return value;
	}
	else
	{
		static_assert(sizeof(T) == 0);
	}
}


enum class network_address_kind : uint8_t
{
	null,
	local,
	ipv4,
	ipv6,
};

class local_address
{
	path_view m_path;

public:
	local_address() = default;

	explicit constexpr local_address(path_view const path)
		: m_path(path)
	{
	}

	constexpr path_view path() const
	{
		return m_path;
	}
};


using network_port_t = uint16_t;

// Note that ipv4_address always represents addresses using native integer endianness.
// This means that for example the loopback address 127.0.0.1 is unambiguously represented as 0x7f'00'00'01.
// Some use cases outside of this library may require explicit conversion to network byte order.
class ipv4_address
{
public:
	using uint_type = uint32_t;

private:
	uint_type m_addr;

public:
	ipv4_address() = default;

	explicit constexpr ipv4_address(uint_type const integer)
		: m_addr(integer)
	{
	}

	constexpr uint_type integer() const
	{
		return m_addr;
	}


	static ipv4_address const unspecified;
	static ipv4_address const localhost;

	static vsm::result<ipv4_address> parse(std::string_view string);


	friend auto operator<=>(ipv4_address const&, ipv4_address const&) = default;
};

inline constexpr ipv4_address ipv4_address::unspecified = ipv4_address(0);
inline constexpr ipv4_address ipv4_address::localhost = ipv4_address(0x7f'00'00'01);

struct ipv4_endpoint
{
	ipv4_address address;
	network_port_t port;


	static vsm::result<ipv4_endpoint> parse(std::string_view string);

	friend auto operator<=>(ipv4_endpoint const&, ipv4_endpoint const&) = default;
};


using ipv6_zone_t = uint32_t;

class ipv6_address
{
public:
	using uint_type = vsm::uint128_t;

private:
	uint_type m_addr;

public:
	ipv6_address() = default;

	explicit constexpr ipv6_address(uint_type const integer)
		: m_addr(integer)
	{
	}

	constexpr uint_type integer() const
	{
		return m_addr;
	}


	static ipv6_address const unspecified;
	static ipv6_address const localhost;

	static vsm::result<ipv6_address> parse(std::string_view string);


	friend auto operator<=>(ipv6_address const&, ipv6_address const&) = default;
};

inline constexpr ipv6_address ipv6_address::unspecified = ipv6_address(0);
inline constexpr ipv6_address ipv6_address::localhost = ipv6_address(1);

struct ipv6_endpoint
{
	ipv6_address address;
	network_port_t port;
	ipv6_zone_t zone;


	static vsm::result<ipv6_endpoint> parse(std::string_view string);

	friend auto operator<=>(ipv6_endpoint const&, ipv6_endpoint const&) = default;
};


class ip_address
{
	network_address_kind m_kind;
	union
	{
		ipv4_address m_ipv4;
		ipv6_address m_ipv6;
	};

public:
	constexpr ip_address(ipv4_address const& address)
		: m_kind(network_address_kind::ipv4)
		, m_ipv4(address)
	{
	}

	constexpr ip_address(ipv6_address const& address)
		: m_kind(network_address_kind::ipv6)
		, m_ipv6(address)
	{
	}


	[[nodiscard]] constexpr network_address_kind kind() const
	{
		return m_kind;
	}
	
	[[nodiscard]] constexpr ipv4_address const& ipv4() const
	{
		vsm_assert(m_kind == network_address_kind::ipv4);
	}
	
	[[nodiscard]] constexpr ipv6_address const& ipv6() const
	{
		vsm_assert(m_kind == network_address_kind::ipv6);
	}
};


struct null_endpoint_t {};
inline constexpr null_endpoint_t null_endpoint = {};

class network_endpoint
{
	network_address_kind m_kind;
	union
	{
		null_endpoint_t m_null;
		local_address m_local;
		ipv4_endpoint m_ipv4;
		ipv6_endpoint m_ipv6;
	};

public:
	constexpr network_endpoint()
		: m_kind(network_address_kind::null)
		, m_null{}
	{
	}

	constexpr network_endpoint(null_endpoint_t)
		: m_kind(network_address_kind::null)
		, m_null{}
	{
	}

	constexpr network_endpoint(local_address const& address)
		: m_kind(network_address_kind::local)
		, m_local(address)
	{
	}

	constexpr network_endpoint(ipv4_endpoint const& endpoint)
		: m_kind(network_address_kind::ipv4)
		, m_ipv4(endpoint)
	{
	}

	constexpr network_endpoint(ipv6_endpoint const& endpoint)
		: m_kind(network_address_kind::ipv6)
		, m_ipv6(endpoint)
	{
	}

	[[nodiscard]] constexpr network_address_kind kind() const
	{
		return m_kind;
	}

	[[nodiscard]] constexpr bool is_null() const
	{
		return m_kind == network_address_kind::null;
	}

	[[nodiscard]] constexpr local_address const& local() const
	{
		vsm_assert(m_kind == network_address_kind::local);
		return m_local;
	}

	[[nodiscard]] constexpr ipv4_endpoint const& ipv4() const
	{
		vsm_assert(m_kind == network_address_kind::ipv4);
		return m_ipv4;
	}

	[[nodiscard]] constexpr ipv6_endpoint const& ipv6() const
	{
		vsm_assert(m_kind == network_address_kind::ipv6);
		return m_ipv6;
	}

	[[nodiscard]] decltype(auto) visit(auto&& visitor) const
	{
		switch (m_kind)
		{
		case network_address_kind::null:
			return std::invoke(vsm_forward(visitor), null_endpoint_t());

		case network_address_kind::local:
			return std::invoke(vsm_forward(visitor), m_local);

		case network_address_kind::ipv4:
			return std::invoke(vsm_forward(visitor), m_ipv4);

		case network_address_kind::ipv6:
			return std::invoke(vsm_forward(visitor), m_ipv6);
		}
	}
};

} // namespace allio
