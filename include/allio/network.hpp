#pragma once

#include <allio/detail/assert.hpp>
#include <allio/detail/bit.hpp>
#include <allio/path_view.hpp>

#include <bit>
#include <concepts>

#include <cstdint>

namespace allio {

template<std::integral T>
constexpr T network_byte_order(T const value)
{
	if constexpr (std::endian::native == std::endian::little)
	{
		return detail::byteswap(value);
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

class ipv4_address
{
	uint32_t m_addr;
	uint16_t m_port;

public:
	constexpr ipv4_address(uint32_t const address, uint16_t const port)
		: m_addr(address)
		, m_port(port)
	{
	}

	constexpr uint32_t address() const
	{
		return m_addr;
	}

	constexpr uint16_t port() const
	{
		return m_port;
	}

	static constexpr ipv4_address localhost(uint16_t const port)
	{
		return ipv4_address(0x7f000001, port);
	}
};

class ipv6_address
{
	uint32_t m_addr[4];

public:
};

class network_address
{
	network_address_kind m_kind;
	union
	{
		struct {} m_null;
		local_address m_local;
		ipv4_address m_ipv4;
		ipv6_address m_ipv6;
	};

public:
	constexpr network_address()
		: m_kind(network_address_kind::null)
		, m_null{}
	{
	}

	constexpr network_address(local_address const& address)
		: m_kind(network_address_kind::local)
		, m_local(address)
	{
	}

	constexpr network_address(ipv4_address const& address)
		: m_kind(network_address_kind::ipv4)
		, m_ipv4(address)
	{
	}

	constexpr network_address(ipv6_address const& address)
		: m_kind(network_address_kind::ipv6)
		, m_ipv6(address)
	{
	}

	constexpr network_address_kind kind() const
	{
		return m_kind;
	}

	constexpr bool is_null() const
	{
		return m_kind == network_address_kind::null;
	}

	constexpr local_address const& local() const
	{
		allio_ASSERT(m_kind == network_address_kind::local);
		return m_local;
	}

	constexpr ipv4_address const& ipv4() const
	{
		allio_ASSERT(m_kind == network_address_kind::ipv4);
		return m_ipv4;
	}

	constexpr ipv6_address const& ipv6() const
	{
		allio_ASSERT(m_kind == network_address_kind::ipv6);
		return m_ipv6;
	}
};

} // namespace allio
