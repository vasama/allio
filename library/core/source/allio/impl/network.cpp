#include <allio/network.hpp>

#include <allio/error.hpp>
#include <allio/network_literals.hpp>

using namespace allio;
using namespace allio::detail;

//TODO: Add from_chars and to_chars interfaces alongside parse.

vsm::result<ipv4_address> ipv4_address::parse(std::string_view const string)
{
	ipv4_address r;
	vsm::literal_parser p(string.data(), string.size());
	if (!_parse_ipv4_address(p, r) || p.beg != p.end)
	{
		return vsm::unexpected(error::invalid_argument);
	}
	return r;
}

vsm::result<ipv4_endpoint> ipv4_endpoint::parse(std::string_view const string)
{
	ipv4_parse_result r = {};
	vsm::literal_parser p(string.data(), string.size());
	if (!_parse_ipv4(p, r) || p.beg != p.end || !r.is_endpoint)
	{
		return vsm::unexpected(error::invalid_argument);
	}
	return r;
}

vsm::result<ipv6_address> ipv6_address::parse(std::string_view const string)
{
	ipv6_address r;
	vsm::literal_parser p(string.data(), string.size());
	if (!_parse_ipv6_address(p, r) || p.beg != p.end)
	{
		return vsm::unexpected(error::invalid_argument);
	}
	return r;
}

vsm::result<ipv6_endpoint> ipv6_endpoint::parse(std::string_view const string)
{
	ipv6_parse_result r = {};
	vsm::literal_parser p(string.data(), string.size());
	if (!_parse_ipv6(p, r) || p.beg != p.end || !r.is_endpoint)
	{
		return vsm::unexpected(error::invalid_argument);
	}
	return r;
}
