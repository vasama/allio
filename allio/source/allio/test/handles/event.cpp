#include <allio/blocking/event.hpp>
#include <allio/senders/event.hpp>

#include <allio/block.hpp>
#include <allio/default_multiplexer.hpp>

#include <vsm/atomic.hpp>

#include <exec/async_scope.hpp>

#include <catch2/catch_all.hpp>

#include <thread>
#include <vector>

using namespace allio;
namespace ex = stdexec;

static bool check_timeout(vsm::result<void> const r)
{
	if (r)
	{
		return true;
	}

	if (r.error().default_error_condition() == std::errc::timed_out)
	{
		return false;
	}

	throw std::system_error(r.error());
}

static bool wait(auto const& event, deadline const deadline = deadline::instant())
{
	return check_timeout(try_block<event_t::wait_t>(event, deadline));
}

static event_mode get_reset_mode(bool const manual_reset)
{
	return manual_reset ? manual_reset_event : auto_reset_event;
}

static event_mode generate_reset_mode()
{
	return get_reset_mode(GENERATE(false, true));
}


/* Blocking events */

TEST_CASE("Events are not signaled on creation by default", "[event][blocking]")
{
	auto const event = blocking::event(generate_reset_mode());
	REQUIRE(event);
	REQUIRE(!wait(event));
}

TEST_CASE("Events can be signaled on creation if requested", "[event][blocking]")
{
	auto const event = blocking::event(generate_reset_mode(), initially_signaled);
	REQUIRE(event);
	REQUIRE(wait(event));
}

TEST_CASE("Manual reset events remain signaled after wait", "[event][blocking]")
{
	auto const event = blocking::event(manual_reset_event, initially_signaled);
	REQUIRE(wait(event));
	REQUIRE(wait(event));
}

TEST_CASE("Auto reset events become unsignaled after wait", "[event][blocking]")
{
	auto const event = blocking::event(auto_reset_event, initially_signaled);
	REQUIRE(wait(event));
	REQUIRE(!wait(event));
}

TEST_CASE("Events can be signaled after creation", "[event][blocking]")
{
	auto const event = blocking::event(generate_reset_mode());
	event.signal();
	REQUIRE(wait(event));
}

TEST_CASE("Events can be reset after being signaled", "[event][blocking]")
{
	auto const event = blocking::event(generate_reset_mode(), initially_signaled);
	event.reset();
	REQUIRE(!wait(event));
}

TEST_CASE("Events can be signaled concurrently with waits", "[event][blocking][threading]")
{
	auto const event = blocking::event(generate_reset_mode());

	std::jthread const signal_thread([&]()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		event.signal();
	});

	REQUIRE(wait(event, deadline::never()));
}

TEST_CASE("Auto reset event signals are only observed by one wait", "[event][blocking][threading]")
{
	//TODO: Pick thread count based on CPU count.
	static constexpr size_t const thread_count = 4;
	static constexpr size_t const signal_count = 10'000;

	auto const event = blocking::event(auto_reset_event);

	std::vector<std::jthread> wait_threads;
	vsm::atomic<bool> stop_requested = false;
	vsm::atomic<size_t> stop_acknowledged_count = 0;

	vsm::atomic<bool> signal_observed = false;
	vsm::atomic<size_t> signals_observed_count = 0;

	// Create waiting threads.
	for (size_t i = 0; i < thread_count; ++i)
	{
		wait_threads.push_back(std::jthread([&]()
		{
			size_t count = 0;

			while (true)
			{
				// Wait for the event to become signaled.
				event.wait();

				if (stop_requested.load(std::memory_order_acquire))
				{
					break;
				}

				signal_observed.store(true, std::memory_order_release);
				++count;
			}

			(void)signals_observed_count.fetch_add(count, std::memory_order_release);
			(void)stop_acknowledged_count.fetch_add(1, std::memory_order_release);
		}));
	}

	// Signal the event in a loop, waiting for confirmation of observation each time.
	for (size_t i = 0; i < signal_count; ++i)
	{
		event.signal();

		// Wait for some thread to observe the signal.
		while (!signal_observed.load(std::memory_order_acquire))
		{
			std::this_thread::yield();
		}

		// Reset the signal observation flag again.
		signal_observed.store(false, std::memory_order_release);
	}

	// Request for all threads to stop.
	stop_requested.store(true, std::memory_order_release);

	// Signal the event until all threads have acknowledged the stop request.
	while (stop_acknowledged_count.load(std::memory_order_acquire) < thread_count)
	{
		event.signal();
		std::this_thread::yield();
	}

	REQUIRE(signals_observed_count.load(std::memory_order_acquire) == signal_count);
}


/* Asynchronous events */

static auto wait_detached(exec::async_scope& scope, auto const& event)
{
	class shared_bool
	{
		std::shared_ptr<bool> m_ptr;

	public:
		explicit shared_bool(std::shared_ptr<bool> ptr)
			: m_ptr(vsm_move(ptr))
		{
		}

		operator bool() const
		{
			return *m_ptr;
		}
	};

	std::shared_ptr<bool> ptr = std::make_shared<bool>(false);
	(void)scope.spawn_future(event.wait() | ex::then([ptr]()
	{
		*ptr = true;
	}));

	return shared_bool(vsm_move(ptr));
}

TEST_CASE("Asynchronous wait on signaled event may complete immediately", "[event][async]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::event(auto_reset_event, initially_signaled).via(multiplexer);

	exec::async_scope scope;
	auto const signaled = wait_detached(scope, event);

	if (!signaled)
	{
		(void)multiplexer.poll(deadline::instant());
	}

	REQUIRE(signaled);
}

TEST_CASE("Asynchronous wait on unsignaled event completes after signaling", "[event][async]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::event(auto_reset_event).via(multiplexer);

	exec::async_scope scope;
	auto const signaled = wait_detached(scope, event);
	REQUIRE(!signaled);

	event.signal();
	(void)multiplexer.poll(deadline::instant());
	REQUIRE(signaled);
}

TEST_CASE("Auto reset event becomes unsignaled after asynchronous wait", "[event][async]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::event(auto_reset_event, initially_signaled).via(multiplexer);

	exec::async_scope scope;
	(void)wait_detached(scope, event);
	(void)multiplexer.poll(deadline::instant());

	REQUIRE(!wait(event));
}

TEST_CASE("Manual reset event remains signaled after asynchronous wait", "[event][async]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::event(manual_reset_event, initially_signaled).via(multiplexer);

	exec::async_scope scope;
	(void)wait_detached(scope, event);
	(void)multiplexer.poll(deadline::instant());

	REQUIRE(wait(event));
}

TEST_CASE("Auto reset event signals are only observed by one asynchronous wait", "[event][async]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::event(auto_reset_event).via(multiplexer);

	exec::async_scope scope;
	auto const signal1 = wait_detached(scope, event);
	auto const signal2 = wait_detached(scope, event);

	event.signal();
	(void)multiplexer.poll();

	bool const is_signaled1 = signal1;
	bool const is_signaled2 = signal2;
	REQUIRE(is_signaled1 != is_signaled2);

	event.signal();
	(void)multiplexer.poll();
}

TEST_CASE("Manual reset event signals are observed by all asynchronous waits", "[event][async]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::event(manual_reset_event).via(multiplexer);

	exec::async_scope scope;
	auto const signal1 = wait_detached(scope, event);
	auto const signal2 = wait_detached(scope, event);

	event.signal();
	(void)multiplexer.poll();

	REQUIRE(signal1);
	REQUIRE(signal2);
}


#if 0 //TODO: Test opaque_handle wrapping event
TEST_CASE("blocking opaque signaling", "[event][opaque_handle][blocking]")
{
	event const event = event(manual_reset_event);
	opaque_handle const opaque = make_opaque_handle(get_opaque_handle(event));

	auto const poll = [&]() -> bool
		{
			return check_timeout(opaque.poll({ .deadline = deadline::instant() }));
		};

	REQUIRE(!poll());

	event.signal();
	REQUIRE(poll());

	event.reset();
	REQUIRE(!poll());

	event.signal();
	REQUIRE(poll());
}
#endif
