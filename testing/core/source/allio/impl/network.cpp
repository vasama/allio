#include <allio/test/network.hpp>

#include <random>

using namespace allio;
using namespace allio::test;

uint16_t test::get_test_network_port()
{
	static constexpr uint16_t min_port = 50000;
	static constexpr uint16_t max_port = 59999;

	static constexpr auto select_random_port = []() -> uint16_t
	{
		std::random_device engine;
		return std::uniform_int_distribution<uint16_t>(min_port, max_port)(engine);
	};

	static uint16_t next_port = select_random_port();

	uint16_t const port = next_port;
	next_port = port == max_port ? min_port : port + 1;
	return port;
}
