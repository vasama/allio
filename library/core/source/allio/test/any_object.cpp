#include <allio/detail/any_object.hpp>

#include <allio/detail/handles/event.hpp>
#include <allio/detail/uniplexer.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;

namespace {

using any_event_t = detail::any_object_t<detail::event_t, detail::uniplexer>;

TEST_CASE("")
{
	
}

} // namespace
