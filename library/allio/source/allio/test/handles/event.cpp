#include <allio/blocking/event.hpp>
#include <allio/senders/event.hpp>

#include <allio/default_multiplexer.hpp>
#include <allio/handles/object.hpp>
#include <allio/nothrow/block.hpp>
#include <allio/senders/sync_wait.hpp>

#include <vsm/atomic.hpp>

#include <exec/async_scope.hpp>

#include <catch2/catch_all.hpp>

#include <thread>
#include <vector>

using namespace allio;
namespace ex = stdexec;

namespace {

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
	return check_timeout(nothrow::block<event_t::wait_t>(event, deadline));
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
	static size_t const thread_count = static_cast<size_t>(std::thread::hardware_concurrency());

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

TEST_CASE("Events can be created inheritable", "[event][blocking]")
{
	auto const event = blocking::event(auto_reset_event, inheritable);
	REQUIRE(event);

	//TODO: Test handle inheritance by spawning another process.
}


/* Asynchronous events */

template<typename T>
class shared_value
{
	using variant_type = std::variant<T, std::exception_ptr>;

	std::shared_ptr<variant_type> m_ptr;

public:
	shared_value()
		: m_ptr(
			std::make_shared<variant_type>(
				std::in_place_type<std::exception_ptr>,
				std::make_exception_ptr(std::runtime_error("Uninitialized shared_value"))))
	{
	}

	template<vsm::no_cvref_of<shared_value> U = T>
		requires std::convertible_to<U, T>
	shared_value(U const& value)
		: m_ptr(std::make_shared<variant_type>(std::in_place_type<T>, vsm_forward(value)))
	{
	}

	template<vsm::cv_convertible_to<T> U>
	shared_value(shared_value<U> const& value)
		: m_ptr(value.m_ptr)
	{
	}

	shared_value(shared_value const&) = default;
	shared_value& operator=(shared_value const&) = default;

	template<vsm::no_cvref_of<shared_value> U = T>
		requires std::convertible_to<U, T>
	shared_value const& operator=(U const& value) const
	{
		m_ptr->template emplace<T>(vsm_forward(value));
		return *this;
	}

	[[nodiscard]] operator T() const
	{
		if (auto const exception = std::get_if<std::exception_ptr>(m_ptr.get()))
		{
			std::rethrow_exception(*exception);
		}

		return *std::get_if<T>(m_ptr.get());
	}

	void set_exception(std::exception_ptr ptr) const
	{
		m_ptr->template emplace<std::exception_ptr>(vsm_move(ptr));
	}

private:
	template<typename U>
	friend class shared_value;
};

static std::exception_ptr make_exception(std::exception_ptr&& exception)
{
	return vsm_move(exception);
}

static std::exception_ptr make_exception(std::error_code const error)
{
	return std::make_exception_ptr(std::system_error(error));
}

static shared_value<bool> wait_detached(exec::async_scope& scope, auto const& event)
{
	shared_value<bool> boolean = false;

	auto handle_value = [boolean]()
	{
		boolean = true;
	};

	auto handle_error = [boolean](auto e)
	{
		boolean.set_exception(make_exception(vsm_move(e)));
	};

	scope.spawn(
		event.wait()
		| ex::then(vsm_move(handle_value))
		| ex::upon_error(vsm_move(handle_error)));

	return boolean;
}


TEST_CASE("Asynchronous wait on signaled event may complete immediately", "[event][async]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::event(auto_reset_event, initially_signaled).via<senders::traits_type>(multiplexer);

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
	auto const event = blocking::event(auto_reset_event).via<senders::traits_type>(multiplexer);

	exec::async_scope scope;
	auto const signaled = wait_detached(scope, event);
	REQUIRE(!signaled);

	event.signal();
	(void)multiplexer.poll(deadline::instant());
	REQUIRE(signaled);
}

TEST_CASE("Auto reset event becomes unsignaled after asynchronous wait", "[event][async][debug]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::event(auto_reset_event, initially_signaled).via<senders::traits_type>(multiplexer);

	exec::async_scope scope;
	(void)wait_detached(scope, event);
	(void)multiplexer.poll(deadline::instant());

	REQUIRE(!wait(event));
}

TEST_CASE("Manual reset event remains signaled after asynchronous wait", "[event][async][debug]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::event(manual_reset_event, initially_signaled).via<senders::traits_type>(multiplexer);

	exec::async_scope scope;
	(void)wait_detached(scope, event);
	(void)multiplexer.poll(deadline::instant());

	REQUIRE(wait(event));
}

TEST_CASE("Auto reset event signals are only observed by one asynchronous wait", "[event][async]")
{
	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::event(auto_reset_event).via<senders::traits_type>(multiplexer);

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
	auto const event = blocking::event(manual_reset_event).via<senders::traits_type>(multiplexer);

	exec::async_scope scope;
	auto const signal1 = wait_detached(scope, event);
	auto const signal2 = wait_detached(scope, event);

	event.signal();
	(void)multiplexer.poll();

	REQUIRE(signal1);
	REQUIRE(signal2);
}

TEST_CASE("Manual reset event can be awaited many times concurrently", "[event][async]")
{
	static constexpr size_t wait_count = 10'000;

	auto multiplexer = default_multiplexer::create().value();
	auto const event = blocking::event(manual_reset_event).via<senders::traits_type>(multiplexer);

	exec::async_scope scope;

	size_t signal_count = 0;
	size_t cancel_count = 0;
	size_t error_count = 0;

	for (size_t i = 0; i < wait_count; ++i)
	{
		scope.spawn(
			event.wait()
			| ex::then([&]() { ++signal_count; })
			| ex::upon_stopped([&]() { ++cancel_count; })
			| ex::upon_error([&](auto) { ++error_count; }));
	}

	if (GENERATE(0, 1))
	{
		// Force I/O submission before signal / cancel:
		while (multiplexer.poll(deadline::instant()).value());
	}

	bool const signal = GENERATE(1, 0);
	bool const cancel = !signal || GENERATE(1, 0);

	if (signal)
	{
		event.signal();
	}

	if (cancel)
	{
		scope.request_stop();
	}

	allio::sync_wait(multiplexer, scope.on_empty());

	if (signal && cancel)
	{
		REQUIRE(signal_count + cancel_count == wait_count);
	}
	else
	{
		REQUIRE(signal_count == (signal ? wait_count : 0));
		REQUIRE(cancel_count == (cancel ? wait_count : 0));
	}

	REQUIRE(error_count == 0);
}

//TODO: Add a stress test for multithreaded cancellation.


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

} // namespace
