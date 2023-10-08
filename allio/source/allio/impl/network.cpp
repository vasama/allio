#include <allio/network.hpp>

#include <allio/error.hpp>
#include <allio/network_literals.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<ipv4_address> ipv4_address::parse(std::string_view const string)
{
	ipv4_parse_result r;
	if (!_parse_ipv4(vsm::literal_parser(string.data(), string.size()), r) || r.is_endpoint)
	{
		return vsm::unexpected(error::invalid_argument);
	}
	return r.address;
}

vsm::result<ipv4_endpoint> ipv4_endpoint::parse(std::string_view const string)
{
	ipv4_parse_result r;
	if (!_parse_ipv4(vsm::literal_parser(string.data(), string.size()), r) || !r.is_endpoint)
	{
		return vsm::unexpected(error::invalid_argument);
	}
	return r;
}

vsm::result<ipv6_address> ipv6_address::parse(std::string_view const string)
{
	ipv6_parse_result r;
	if (!_parse_ipv6(vsm::literal_parser(string.data(), string.size()), r) || r.is_endpoint)
	{
		return vsm::unexpected(error::invalid_argument);
	}
	return r.address;
}

vsm::result<ipv6_endpoint> ipv6_endpoint::parse(std::string_view const string)
{
	ipv6_parse_result r;
	if (!_parse_ipv6(vsm::literal_parser(string.data(), string.size()), r) || !r.is_endpoint)
	{
		return vsm::unexpected(error::invalid_argument);
	}
	return r;
}
