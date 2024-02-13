#pragma once

#include <allio/network.hpp>

#include <vsm/literals.hpp>

namespace allio {
namespace detail {

struct ipv4_parse_result : ipv4_endpoint
{
	bool is_endpoint;
};

constexpr bool _parse_ipv4_address(vsm::literal_parser& p, ipv4_address& r)
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

	r = ipv4_address(integer);
	return true;
}

constexpr bool _parse_ipv4(vsm::literal_parser& p, ipv4_parse_result& r)
{
	if (!_parse_ipv4_address(p, r.address))
	{
		return false;
	}

	if (p.consume(':'))
	{
		if (!p.consume_integer(r.port))
		{
			return false;
		}
		r.is_endpoint = true;
	}

	return true;
}

consteval ipv4_parse_result _parse_ipv4_consteval(char const* const data, size_t const size)
{
	ipv4_parse_result r = {};
	vsm::literal_parser p(data, size);
	if (!_parse_ipv4(p, r) || p.beg != p.end)
	{
		throw 0;
	}
	return r;
}


struct ipv6_parse_result : ipv6_endpoint
{
	bool is_endpoint;
};

constexpr bool _parse_ipv6_integer(vsm::literal_parser& p, uint16_t& value)
{
	size_t size = p.end - p.beg;
	if (size > 4)
	{
		size = 4;
	}
	vsm::literal_parser q(p.beg, size);

	if (!q.consume_integer(value, 0x10))
	{
		return false;
	}

	p.beg = q.beg;
	return true;
}

constexpr bool _parse_ipv6_address(vsm::literal_parser& p, ipv6_address& r)
{
	uint16_t hexadectets[8] = {};

	int hexadectet_count = 0;
	int double_colon_index = -1;

	if (p.consume(':'))
	{
		double_colon_index = 0;
	}
	else
	{
		if (!_parse_ipv6_integer(p, hexadectets[hexadectet_count]))
		{
			return false;
		}

		hexadectet_count = 1;
	}

	for (; hexadectet_count < 8; ++hexadectet_count)
	{
		if (!p.consume(':'))
		{
			break;
		}

		if (p.consume(':'))
		{
			if (double_colon_index >= 0)
			{
				return false;
			}

			double_colon_index = hexadectet_count;
		}

		if (!_parse_ipv6_integer(p, hexadectets[hexadectet_count]))
		{
			if (hexadectet_count == double_colon_index)
			{
				break;
			}

			return false;
		}
	}

	if (double_colon_index >= 0)
	{
		// If a double colon is used, one to seven hexadectets may be specified.
		if (hexadectet_count == 0 || hexadectet_count == 8)
		{
			return false;
		}

		// Shift the hexadectets after the double colon to the back of the array.
		for (int i = 7, j = hexadectet_count - 1; j >= double_colon_index; --i, --j)
		{
			hexadectets[i] = hexadectets[j];
			hexadectets[j] = 0;
		}
	}
	else
	{
		// If a double colon is not used, all eight hexadectets must be specified.
		if (hexadectet_count != 8)
		{
			return false;
		}
	}

	ipv6_address::uint_type const h64 =
		static_cast<uint64_t>(hexadectets[0]) << 48 |
		static_cast<uint64_t>(hexadectets[1]) << 32 |
		static_cast<uint64_t>(hexadectets[2]) << 16 |
		static_cast<uint64_t>(hexadectets[3]);

	ipv6_address::uint_type const l64 =
		static_cast<uint64_t>(hexadectets[4]) << 48 |
		static_cast<uint64_t>(hexadectets[5]) << 32 |
		static_cast<uint64_t>(hexadectets[6]) << 16 |
		static_cast<uint64_t>(hexadectets[7]);

	r = ipv6_address(h64 << 64 | l64);

	return true;
}

constexpr bool _parse_ipv6(vsm::literal_parser& p, ipv6_parse_result& r)
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

		if (!p.consume(':'))
		{
			return false;
		}

		if (!p.consume_integer(r.port))
		{
			return false;
		}
	}

	return true;
}

consteval ipv6_parse_result _parse_ipv6_consteval(char const* const data, size_t const size)
{
	ipv6_parse_result r = {};
	vsm::literal_parser p(data, size);
	if (!_parse_ipv6(p, r) || p.beg != p.end)
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
