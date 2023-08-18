#include <allio/event_handle.hpp>

#include <vsm/atomic.hpp>

#include <catch2/catch_all.hpp>

#include <thread>
#include <vector>

using namespace allio;

static vsm::result<bool> wait(event_handle const& event, deadline const deadline)
{
	auto const r = event.wait(
	{
		.deadline = deadline,
	});

	if (r)
	{
		return true;
	}

	if (r.error().default_error_condition() == std::errc::timed_out)
	{
		return false;
	}

	return vsm::unexpected(r.error());
}


TEST_CASE("basic signaling", "[event_handle]")
{
	bool const manual_reset = GENERATE(0, 1);
	bool is_signaled = GENERATE(0, 1);

	event_handle const event = create_event(
		manual_reset ? event_handle::manual_reset : event_handle::auto_reset,
	{
		.signal = is_signaled,
	}).value();

	auto const wait = [&]()
	{
		return ::wait(event, deadline::instant());
	};

	if (GENERATE(0, 1))
	{
		event.signal().value();
		is_signaled = true;
	}

	REQUIRE(wait() == is_signaled);
	is_signaled &= manual_reset;

	REQUIRE(wait() == is_signaled);
	is_signaled &= manual_reset;

	event.reset().value();
	is_signaled = false;

	REQUIRE(wait() == is_signaled);
	is_signaled &= manual_reset;

	if (GENERATE(0, 1))
	{
		event.signal().value();
		is_signaled = true;
	}

	REQUIRE(wait() == is_signaled);
	is_signaled &= manual_reset;

	REQUIRE(wait() == is_signaled);
	is_signaled &= manual_reset;
}

TEST_CASE("concurrent signaling", "[event_handle]")
{
	event_handle const event = create_event(event_handle::manual_reset).value();

	std::jthread const signal_thread([&]()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		event.signal().value();
	});

	REQUIRE(wait(event, std::chrono::milliseconds(1000)));
}

TEST_CASE("concurrent fighting", "[event_handle]")
{
	//TODO: Pick thread count based on CPU count.
	static constexpr size_t const thread_count = 4;
	static constexpr size_t const signal_count = 100'000;

	event_handle const event = create_event(event_handle::auto_reset).value();

	std::vector<std::jthread> wait_threads;
	std::vector<std::jthread> reset_threads;
	vsm::atomic<bool> observed = false;

	vsm::atomic<bool> stop_requested = false;
	vsm::atomic<size_t> stop_acknowledged = 0;
	vsm::atomic<size_t> signals_observed = 0;

	// Create waiting threads.
	for (size_t i = 0; i < thread_count; ++i)
	{
		wait_threads.push_back(std::jthread([&]()
		{
			size_t count = 0;

			while (true)
			{
				event.wait().value();

				if (stop_requested.load(std::memory_order_acquire))
				{
					break;
				}

				observed.store(true, std::memory_order_release);
				++count;
			}

			(void)signals_observed.fetch_add(count, std::memory_order_release);
			(void)stop_acknowledged.fetch_add(1, std::memory_order_release);
		}));
	}

	// Create resetting threads.
	for (size_t i = 0; i < thread_count; ++i)
	{
		reset_threads.push_back(std::jthread([&]()
		{
			while (!stop_requested.load(std::memory_order_acquire))
			{
				event.reset().value();
				std::this_thread::yield();
			}
		}));
	}

	// Signal the event in a loop, waiting for confirmation each time.
	for (size_t i = 0; i < signal_count; ++i)
	{
		event.signal().value();
		while (!observed.load(std::memory_order_acquire))
		{
			std::this_thread::yield();
		}
		observed.store(false, std::memory_order_release);
	}

	// Request and wait for all the threads to stop.
	stop_requested.store(true, std::memory_order_release);
	while (stop_acknowledged.load(std::memory_order_acquire) < thread_count)
	{
		event.signal().value();
		std::this_thread::yield();
	}

	REQUIRE(signals_observed.load(std::memory_order_acquire) == signal_count);
}
