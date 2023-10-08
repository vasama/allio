#include <allio/network.hpp>
#include <allio/network_literals.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::network_literals;

static_assert("127.0.0.1"_ipv4 == ipv4_address::localhost);
static_assert("127.0.0.1:80"_ipv4 == ipv4_endpoint{ ipv4_address::localhost, 80 });

static_assert("::1"_ipv6 == ipv6_address::localhost);
static_assert("[::1]:80"_ipv6 == ipv6_endpoint{ ipv6_address::localhost, 80 });

TEST_CASE("IPv4 addresses can be parsed from text", "[network]")
{
	REQUIRE(ipv4_address::parse("127.0.0.1").value() == ipv4_address::localhost);
	REQUIRE(ipv4_endpoint::parse("127.0.0.1:80").value() == ipv4_endpoint{ ipv4_address::localhost, 80 });
}

TEST_CASE("IPv6 addresses can be parsed from text", "[network]")
{
	REQUIRE(ipv6_address::parse("::1").value() == ipv6_address::localhost);
	REQUIRE(ipv6_endpoint::parse("[::1]:80").value() == ipv6_endpoint{ ipv6_address::localhost, 80 });
}
