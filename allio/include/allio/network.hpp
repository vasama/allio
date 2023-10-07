#pragma once

#include <allio/path_view.hpp>

#include <vsm/assert.h>
#include <vsm/utility.hpp>

#include <bit>
#include <concepts>

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
	explicit local_address(path_view const path)
		: m_path(path)
	{
	}

	path_view path() const
	{
		return m_path;
	}
};

// Note that ipv4_address always represents addresses using native integer endianness.
// This means that for example the loopback address 127.0.0.1 is unambiguously represented as 0x7f'00'00'01.
// Some use cases outside of this library may require explicit conversion to network byte order.
class ipv4_address
{
	uint32_t m_addr;

public:
	explicit constexpr ipv4_address(uint32_t const address)
		: m_addr(address)
	{
	}

	constexpr uint32_t integer() const
	{
		return m_addr;
	}


	static ipv4_address const localhost;
};

inline constexpr ipv4_address ipv4_address::localhost = ipv4_address(0x7f'00'00'01);

class ipv6_address
{
	uint32_t m_addr[4];

public:
	static ipv6_address const localhost;
};


template<typename Address>
struct ip_endpoint
{
	Address address;
	uint16_t port;
};

using ipv4_endpoint = ip_endpoint<ipv4_address>;
using ipv6_endpoint = ip_endpoint<ipv6_address>;


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
