#include <allio/event_queue.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;

TEST_CASE("event_queue", "[event_queue]")
{
	default_multiplexer multiplexer = default_multiplexer::create().value();
	event_queue queue(&multiplexer);

	
}
