#include <allio/detail/any_object.hpp>

#include <allio/nothrow/event.hpp>
#include <allio/detail/uniplexer.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;

namespace {

using any_event_t = detail::any_object_t<detail::event_t, detail::uniplexer>;

TEST_CASE("any_object")
{
	auto event = nothrow::event(event_mode::auto_reset).value();
	auto any_event = detail::make_any<any_event_t>(vsm_move(event)).value();

	detail::blocking_io<event_t::signal_t>(
		any_event,
		detail::io_parameters_t<any_event_t, event_t::signal_t>()).value();
}

} // namespace
