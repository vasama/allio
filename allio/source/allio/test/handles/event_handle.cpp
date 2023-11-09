#include <allio/event_handle.hpp>

#include <vsm/atomic.hpp>

#include <exec/async_scope.hpp>

#include <catch2/catch_all.hpp>

#include <thread>
#include <vector>

using namespace allio;
namespace ex = stdexec;

static vsm::result<bool> check_timeout(vsm::result<void> const r)
{
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

static vsm::result<bool> wait(abstract_event_handle const& event, deadline const deadline = deadline::instant())
{
	return check_timeout(detail::blocking_io<event_t::wait_t>(
		event,
		detail::io_args<event_t::wait_t>()(deadline)));
}

static event_mode get_reset_mode(bool const manual_reset)
{
	return manual_reset ? manual_reset_event : auto_reset_event;
}

static event_mode generate_reset_mode()
{
	return get_reset_mode(GENERATE(false, true));
}

TEST_CASE("new event_handle is not signaled", "[event_handle][blocking]")
{
	auto const event = blocking::create_event(generate_reset_mode()).value();
	REQUIRE(event);
	REQUIRE(!wait(event).value());
}

TEST_CASE("new event_handle is signaled if requested", "[event_handle][blocking]")
{
	auto const event = blocking::create_event(generate_reset_mode(), signal_event).value();
	REQUIRE(event);
	REQUIRE(wait(event).value());
}

TEST_CASE("auto reset event_handle becomes unsignaled after blocking wait", "[event_handle][blocking]")
{
	auto const event = blocking::create_event(auto_reset_event, signal_event).value();
	REQUIRE(wait(event).value());
	REQUIRE(!wait(event).value());
}

TEST_CASE("manual reset event_handle remains signaled after blocking wait", "[event_handle][blocking]")
{
	auto const event = blocking::create_event(manual_reset_event, signal_event).value();
	REQUIRE(wait(event).value());
	REQUIRE(wait(event).value());
}

TEST_CASE("event_handle can be signaled after creation", "[event_handle][blocking]")
{
	auto const event = blocking::create_event(generate_reset_mode()).value();
	event.signal().value();
	REQUIRE(wait(event).value());
}

TEST_CASE("signaled event_handle can be reset", "[event_handle][blocking]")
{
	auto const event = blocking::create_event(generate_reset_mode(), signal_event).value();
	event.reset().value();
	REQUIRE(!wait(event).value());
}

TEST_CASE("event_handle can be signaled concurrently", "[event_handle][blocking][threading]")
{
	auto const event = blocking::create_event(generate_reset_mode()).value();

	std::jthread const signal_thread([&]()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		event.signal().value();
	});

	REQUIRE(wait(event, deadline::never()));
}

TEST_CASE("auto reset event_handle signals are only observed by one wait", "[event_handle][blocking][threading]")
{
	//TODO: Pick thread count based on CPU count.
	static constexpr size_t const thread_count = 4;
	static constexpr size_t const signal_count = 10'000;

	auto const event = blocking::create_event(auto_reset_event).value();

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
				event.wait().value();

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
		event.signal().value();

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
		event.signal().value();
		std::this_thread::yield();
	}

	REQUIRE(signals_observed_count.load(std::memory_order_acquire) == signal_count);
}


template<typename MultiplexerHandle>
static auto wait_detached(
	exec::async_scope& scope,
	async::basic_event_handle<MultiplexerHandle> const& event)
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

TEST_CASE("async wait on signaled event_handle may complete immediately", "[event_handle][async]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::create_event(auto_reset_event, signal_event).value().via(multiplexer).value();

	exec::async_scope scope;
	auto const signaled = wait_detached(scope, event);

	if (!signaled)
	{
		(void)multiplexer.poll(deadline::instant()).value();
	}

	REQUIRE(signaled);
}

TEST_CASE("async wait on unsignaled event_handle completes after signaling", "[event_handle][async]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::create_event(auto_reset_event).value().via(multiplexer).value();

	exec::async_scope scope;
	auto const signaled = wait_detached(scope, event);
	REQUIRE(!signaled);

	event.signal().value();
	(void)multiplexer.poll(deadline::instant()).value();
	REQUIRE(signaled);
}

TEST_CASE("auto reset event_handle becomes unsignaled after async wait", "[event_handle][async]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::create_event(auto_reset_event, signal_event).value().via(multiplexer).value();

	exec::async_scope scope;
	(void)wait_detached(scope, event);
	(void)multiplexer.poll(deadline::instant()).value();

	REQUIRE(!wait(event).value());
}

TEST_CASE("manual reset event_handle remains signaled after async wait", "[event_handle][async]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::create_event(manual_reset_event, signal_event).value().via(multiplexer).value();

	exec::async_scope scope;
	(void)wait_detached(scope, event);
	(void)multiplexer.poll(deadline::instant()).value();

	REQUIRE(wait(event).value());
}

TEST_CASE("auto reset event_handle signals are only observed by one async wait", "[event_handle][async]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::create_event(auto_reset_event).value().via(multiplexer).value();

	exec::async_scope scope;
	auto const signal1 = wait_detached(scope, event);
	auto const signal2 = wait_detached(scope, event);

	event.signal().value();
	(void)multiplexer.poll().value();

	bool const is_signaled1 = signal1;
	bool const is_signaled2 = signal2;
	REQUIRE(is_signaled1 != is_signaled2);

	event.signal().value();
	(void)multiplexer.poll().value();
}

TEST_CASE("manual reset event_handle signals are observed by all async waits", "[event_handle][async]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::create_event(manual_reset_event).value().via(multiplexer).value();

	exec::async_scope scope;
	auto const signal1 = wait_detached(scope, event);
	auto const signal2 = wait_detached(scope, event);

	event.signal().value();
	(void)multiplexer.poll().value();

	REQUIRE(signal1);
	REQUIRE(signal2);
}


#if 0 //TODO: Test opaque_handle wrapping event_handle
TEST_CASE("blocking opaque signaling", "[event_handle][opaque_handle][blocking]")
{
	event_handle const event = create_event(manual_reset_event).value();
	opaque_handle const opaque = make_opaque_handle(get_opaque_handle(event));

	auto const poll = [&]() -> bool
		{
			return check_timeout(opaque.poll({ .deadline = deadline::instant() })).value();
		};

	REQUIRE(!poll());

	event.signal().value();
	REQUIRE(poll());

	event.reset().value();
	REQUIRE(!poll());

	event.signal().value();
	REQUIRE(poll());
}
#endif
