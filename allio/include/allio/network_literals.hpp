#pragma once

#include <allio/network.hpp>

#include <vsm/literals.hpp>

namespace allio {
namespace detail {

struct ipv4_parse_result : ipv4_endpoint
{
	bool is_endpoint;
};

constexpr bool _parse_ipv4(vsm::literal_parser p, ipv4_parse_result& r)
{
	uint32_t integer = 0;

	uint8_t octet;
	if (!p.consume_integer(octet) || !p.consume('.'))
	{
		return false;
	}
	integer = integer << 8 | octet;
	if (!p.consume_integer(octet) || !p.consume('.'))
	{
		return false;
	}
	integer = integer << 8 | octet;
	if (!p.consume_integer(octet) || !p.consume('.'))
	{
		return false;
	}
	integer = integer << 8 | octet;
	if (!p.consume_integer(octet))
	{
		return false;
	}
	integer = integer << 8 | octet;
	r.address = ipv4_address(integer);

	if (r.is_endpoint = p.consume(':'))
	{
		if (!p.consume_integer(r.port))
		{
			return false;
		}
	}

	return p.beg == p.end;
}

consteval ipv4_parse_result _parse_ipv4_consteval(char const* const data, size_t const size)
{
	ipv4_parse_result r = {};
	if (!_parse_ipv4(vsm::literal_parser(data, size), r))
	{
		throw 0;
	}
	return r;
}


struct ipv6_parse_result : ipv6_endpoint
{
	bool is_endpoint;
};

constexpr bool _parse_ipv6_address(vsm::literal_parser& p, ipv6_address& r)
{
	uint16_t h[8] = {};

	int i = 0;
	int j = -1;

	if (p.consume(':'))
	{
		j = 0;
	}
	else if (!p.consume_integer(h[i++], 0x10))
	{
		return false;
	}

	for (; i < 8; ++i)
	{
		if (!p.consume(':'))
		{
			break;
		}

		if (p.consume(':') && j >= 0)
		{
			return false;
		}

		if (!p.consume_integer(h[i++], 0x10))
		{
			return false;
		}
	}

	// If a double colon was  used.
	if (j >= 0)
	{
		// A double colon cannot be used if all eight hextets are specified.
		if (i == 8)
		{
			return false;
		}

		int k = 7;

		for (; i >= j; --k, --i)
		{
			h[k] = h[i];
		}

		for (; k >= j; --k)
		{
			h[k] = 0;
		}
	}

	using vsm::uint128_t;

	uint128_t const h64 =
		static_cast<uint64_t>(h[0]) << 48 |
		static_cast<uint64_t>(h[1]) << 32 |
		static_cast<uint64_t>(h[2]) << 16 |
		static_cast<uint64_t>(h[3]);

	uint128_t const l64 =
		static_cast<uint64_t>(h[4]) << 48 |
		static_cast<uint64_t>(h[5]) << 32 |
		static_cast<uint64_t>(h[6]) << 16 |
		static_cast<uint64_t>(h[7]);

	r = ipv6_address(h64 << 64 | l64);

	return true;
}

constexpr bool _parse_ipv6(vsm::literal_parser p, ipv6_parse_result& r)
{
	r.is_endpoint = p.consume('[');

	if (!_parse_ipv6_address(p, r.address))
	{
		return false;
	}

	if (r.is_endpoint)
	{
		if (p.consume('%'))
		{
			if (!p.consume_integer(r.zone))
			{
				return false;
			}
		}

		if (!p.consume(']'))
		{
			return false;
		}

		if (p.consume(':'))
		{
			if (!p.consume_integer(r.port))
			{
				return false;
			}
		}
	}

	return p.beg == p.end;
}

consteval ipv6_parse_result _parse_ipv6_consteval(char const* const data, size_t const size)
{
	ipv6_parse_result r = {};
	if (!_parse_ipv6(vsm::literal_parser(data, size), r))
	{
		throw 0;
	}
	return r;
}

} // namespace detail

namespace network_literals {

template<vsm::literal_string literal>
consteval auto operator""_ipv4()
{
	constexpr auto info = detail::_parse_ipv4_consteval(literal.data, literal.size);
	if constexpr (info.is_endpoint)
	{
		return static_cast<ipv4_endpoint const&>(info);
	}
	else
	{
		return info.address;
	}
}

template<vsm::literal_string literal>
consteval auto operator""_ipv6()
{
	constexpr auto info = detail::_parse_ipv6_consteval(literal.data, literal.size);
	if constexpr (info.is_endpoint)
	{
		return static_cast<ipv6_endpoint const&>(info);
	}
	else
	{
		return info.address;
	}
}

} // namespace network_literals
} // namespace allio
