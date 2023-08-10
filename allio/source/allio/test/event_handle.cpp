#include <allio/event_handle.hpp>

#include <allio/test/multiplexer.hpp>

#include <vsm/atomic.hpp>

#include <catch2/catch_all.hpp>

#include <thread>

using namespace allio;

static vsm::result<bool> unwrap_timeout(vsm::result<void> const& r)
{
	if (r)
	{
		return true;
	}

	if (r.error().default_error_condition() == std::error_condition(std::errc::timed_out))
	{
		return false;
	}
	
	return vsm::unexpected(r.error());
}

static std::jthread signal_thread(event_handle const& event)
{
	return std::jthread([&event]()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		event.signal().value();
	});
}

static std::optional<std::jthread> maybe_signal_thread(event_handle const& event)
{
	if (GENERATE(0, 1))
	{
		return signal_thread(event);
	}

	return std::nullopt;
}

static bool maybe_reset(event_handle const& event)
{
	if (GENERATE(0, 1))
	{
		event.reset().value();
		return true;
	}

	return event.get_flags()[event_handle::flags::auto_reset];
}

TEST_CASE("event_handle", "[event_handle]")
{
	auto event = create_event(
	{
		.auto_reset = GENERATE(false, true),
	}).value();

	// Wait 1.
	{
		auto const t = maybe_signal_thread(event);
		bool const is_signaled = static_cast<bool>(t);

		bool const r = unwrap_timeout(event.wait(
		{
			.deadline = deadline(std::chrono::milliseconds(10)),
		})).value();

		REQUIRE(r == is_signaled);
	}

	// Wait 2.
	{
		bool const is_reset = maybe_reset(event);

		auto const t = maybe_signal_thread(event);
		bool const is_signaled = static_cast<bool>(t) || !is_reset;

		bool const r = unwrap_timeout(event.wait(
		{
			.deadline = deadline(std::chrono::milliseconds(10)),
		})).value();

		REQUIRE(r == is_signaled);
	}
}
