#include <allio/event_handle.hpp>

#include <allio/sync_wait.hpp>

#include <vsm/atomic.hpp>

#include <exec/async_scope.hpp>
#include <exec/single_thread_context.hpp>

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

static vsm::result<bool> wait(blocking_event_handle const& event, deadline const deadline = deadline::instant())
{
	return check_timeout(event.wait(deadline));
}

static event_reset_mode get_reset_mode(bool const manual_reset)
{
	return manual_reset ? manual_reset_event : auto_reset_event;
}

static event_reset_mode generate_reset_mode()
{
	return get_reset_mode(GENERATE(false, true));
}


TEST_CASE("new event_handle is not signaled", "[event_handle][blocking][create]")
{
	auto const event = create_event(generate_reset_mode()).value();
	REQUIRE(event);
	REQUIRE(!wait(event).value());
}

TEST_CASE("new event_handle is signaled if requested", "[event_handle][blocking][create]")
{
	auto const event = create_event(generate_reset_mode(), signal_event).value();
	REQUIRE(event);
	REQUIRE(wait(event).value());
}

TEST_CASE("auto reset event_handle becomes unsignaled", "[event_handle][blocking]")
{
	auto const event = create_event(auto_reset_event, signal_event).value();
	REQUIRE(wait(event).value());
	REQUIRE(!wait(event).value());
}

TEST_CASE("manual reset event_handle remains signaled", "[event_handle][blocking]")
{
	auto const event = create_event(manual_reset_event, signal_event).value();
	REQUIRE(wait(event).value());
	REQUIRE(wait(event).value());
}

TEST_CASE("event_handle can be signaled", "[event_handle][blocking]")
{
	auto const event = create_event(generate_reset_mode()).value();
	event.signal().value();
	REQUIRE(wait(event).value());
}

TEST_CASE("signaled event_handle can be reset", "[event_handle][blocking]")
{
	auto const event = create_event(generate_reset_mode(), signal_event).value();
	event.reset().value();
	REQUIRE(!wait(event).value());
}

TEST_CASE("event_handle can be signaled concurrently", "[event_handle][blocking][threading]")
{
	auto const event = create_event(generate_reset_mode()).value();

	std::jthread const signal_thread([&]()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		event.signal().value();
	});

	REQUIRE(wait(event, deadline::never()));
}

TEST_CASE("auto reset event_handle signals are only observed by one waiter", "[event_handle][blocking][threading]")
{
	//TODO: Pick thread count based on CPU count.
	static constexpr size_t const thread_count = 4;
	static constexpr size_t const signal_count = 10'000;

	auto const event = create_event(auto_reset_event).value();

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


TEST_CASE("async signaling", "[event_handle][async]")
{
	bool const manual_reset = GENERATE(0, 1);
	CAPTURE(manual_reset);

	default_multiplexer multiplexer = default_multiplexer::create().value();
	auto const event = create_event(&multiplexer, get_reset_mode(manual_reset)).value();

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

	auto const wait_detached = [&](exec::async_scope& scope) -> shared_bool
	{
		std::shared_ptr<bool> ptr = std::make_shared<bool>(false);
		(void)scope.spawn_future(event.wait_async() | ex::then([ptr]()
		{
			*ptr = true;
		}));
		return shared_bool(vsm_move(ptr));
	};

	{
		exec::async_scope scope;
		auto const signaled = wait_detached(scope);
		REQUIRE(!signaled);

		(void)multiplexer.poll({ .deadline = deadline::instant() }).value();
		REQUIRE(!signaled);

		event.signal().value();
		REQUIRE(!signaled);

		(void)multiplexer.poll({ .deadline = deadline::instant() }).value();
		REQUIRE(signaled);

		(void)multiplexer.poll({ .deadline = deadline::instant() }).value();
		REQUIRE(signaled);
	}

	{
		exec::async_scope scope;
		auto const signaled = wait_detached(scope);

		if (!manual_reset)
		{
			REQUIRE(!signaled);
		}

		(void)multiplexer.poll({ .deadline = deadline::instant() }).value();
		REQUIRE(signaled == manual_reset);

		if (!manual_reset)
		{
			event.signal().value();
			REQUIRE(!signaled);
		}

		(void)multiplexer.poll({ .deadline = deadline::instant() }).value();
		REQUIRE(signaled);

		(void)multiplexer.poll({ .deadline = deadline::instant() }).value();
		REQUIRE(signaled);
	}

	{
		event.reset().value();

		exec::async_scope scope;
		auto const signaled = wait_detached(scope);
		REQUIRE(!signaled);

		(void)multiplexer.poll({ .deadline = deadline::instant() }).value();
		REQUIRE(!signaled);

		event.signal().value();
		REQUIRE(!signaled);

		(void)multiplexer.poll({ .deadline = deadline::instant() }).value();
		REQUIRE(signaled);

		(void)multiplexer.poll({ .deadline = deadline::instant() }).value();
		REQUIRE(signaled);
	}
}

#if 0
TEST_CASE("async concurrent signaling", "[event_handle][async][threading]")
{
	default_multiplexer multiplexer = default_multiplexer::create().value();
	auto const event = create_event(&multiplexer, generate_reset_mode()).value();

	exec::single_thread_context signal_context;
	auto signal_sender = ex::on(signal_context.get_scheduler(), ex::just() | ex::then([&event]()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		event.signal().value();
	}));

	//FIXME: The when_all completes on the signal thread.
	//       This leaves sync_wait stuck polling the multiplexer.
	sync_wait(multiplexer, ex::when_all(event.wait_async(), signal_sender));
}
#endif


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
