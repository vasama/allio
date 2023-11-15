#include <allio/network.hpp>
#include <allio/network_literals.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::network_literals;

TEST_CASE("IPv4 addresses can be parsed from text", "[network]")
{
	static_assert(std::is_same_v<decltype("127.0.0.1"_ipv4), ipv4_address>);
	static_assert(std::is_same_v<decltype("127.0.0.1:80"_ipv4), ipv4_endpoint>);

	REQUIRE(ipv4_address::parse("0.0.0.0").value() == ipv4_address::unspecified);
	REQUIRE(ipv4_address::parse("127.0.0.1").value() == ipv4_address::localhost);
	REQUIRE(ipv4_address::parse("255.255.255.255").value() == ipv4_address(0xff'ff'ff'ff));
	REQUIRE(ipv4_address::parse("123.45.67.89").value() == ipv4_address(0x7b'2d'43'59));
	REQUIRE(!ipv4_address::parse("127.0.0.256"));
	REQUIRE(!ipv4_address::parse("127.0.0.1:80"));

	REQUIRE(ipv4_endpoint::parse("127.0.0.1:80").value() == ipv4_endpoint{ ipv4_address::localhost, 80 });
	REQUIRE(ipv4_endpoint::parse("127.0.0.1:0").value() == ipv4_endpoint{ ipv4_address::localhost, 0 });
	REQUIRE(ipv4_endpoint::parse("127.0.0.1:65535").value() == ipv4_endpoint{ ipv4_address::localhost, 0xffff });
	REQUIRE(!ipv4_endpoint::parse("127.0.0.1"));
	REQUIRE(!ipv4_endpoint::parse("127.0.0.1:-0"));
	REQUIRE(!ipv4_endpoint::parse("127.0.0.1:+0"));
	REQUIRE(!ipv4_endpoint::parse("127.0.0.1:-1"));
	REQUIRE(!ipv4_endpoint::parse("127.0.0.1:+1"));
	REQUIRE(!ipv4_endpoint::parse("127.0.0.1:65536"));
}

TEST_CASE("IPv6 addresses can be parsed from text", "[network]")
{
	static_assert(std::is_same_v<decltype("::1"_ipv6), ipv6_address>);
	static_assert(std::is_same_v<decltype("[::1]:80"_ipv6), ipv6_endpoint>);

	static constexpr auto make_address = [&](std::same_as<int> auto const... i)
	{
		return ipv6_address((ipv6_address::uint_type(0) | ... | (ipv6_address::uint_type(1) << i)));
	};

	REQUIRE(ipv6_address::parse("::0").value() == ipv6_address::unspecified);
	REQUIRE(ipv6_address::parse("::1").value() == ipv6_address::localhost);
	REQUIRE(ipv6_address::parse("::1:1").value() == make_address(16, 0));
	REQUIRE(ipv6_address::parse("1::").value() == make_address(112));
	REQUIRE(ipv6_address::parse("1::1").value() == make_address(112, 0));
	REQUIRE(ipv6_address::parse("1:1::").value() == make_address(112, 96));
	REQUIRE(ipv6_address::parse("0:1::1:0").value() == make_address(96, 16));
	REQUIRE(
		ipv6_address::parse("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff").value() ==
		ipv6_address(~ipv6_address::uint_type(0)));
	REQUIRE(
		ipv6_address::parse("2001:DB8:AC10:FE01::").value() ==
		ipv6_address(ipv6_address::uint_type(0x2001'0DB8'AC10'FE01) << 64));
	REQUIRE(
		ipv6_address::parse("::0123:4567:89AB:CDEF").value() ==
		ipv6_address(ipv6_address::uint_type(0x0123'4567'89AB'CDEF)));
	REQUIRE(!ipv6_address::parse("::10000"));
	REQUIRE(!ipv6_address::parse("1"));
	REQUIRE(!ipv6_address::parse("::"));
	REQUIRE(!ipv6_address::parse(":::1"));
	REQUIRE(!ipv6_address::parse("::0::0"));
	REQUIRE(!ipv6_address::parse("[::1]"));
	REQUIRE(!ipv6_address::parse("[::1]:80"));

	REQUIRE(ipv6_endpoint::parse("[::1]:80").value() == ipv6_endpoint{ ipv6_address::localhost, 80 });
	REQUIRE(ipv6_endpoint::parse("[::1]:0").value() == ipv6_endpoint{ ipv6_address::localhost, 0 });
	REQUIRE(ipv6_endpoint::parse("[::1]:65535").value() == ipv6_endpoint{ ipv6_address::localhost, 0xffff });
	REQUIRE(ipv6_endpoint::parse("[::1%1]:80").value() == ipv6_endpoint{ ipv6_address::localhost, 80, 1 });
	REQUIRE(!ipv6_endpoint::parse("[::1]"));
	REQUIRE(!ipv6_endpoint::parse("[::1]:-0"));
	REQUIRE(!ipv6_endpoint::parse("[::1]:+0"));
	REQUIRE(!ipv6_endpoint::parse("[::1]:-1"));
	REQUIRE(!ipv6_endpoint::parse("[::1]:+1"));
	REQUIRE(!ipv6_endpoint::parse("[::1]:65536"));
}
