#include <allio/blocking/pipe.hpp>
#include <allio/senders/pipe.hpp>

#include <allio/senders/sync_wait.hpp>
#include <allio/senders/task.hpp>
#include <allio/test/match_error.hpp>
#include <allio/test/spawn.hpp>

#include <exec/async_scope.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
namespace ex = stdexec;

namespace {

//TODO: Investigate the possibility of a "test suite template" in Catch 2. If nothing else, it is
//      possible using an inline file parameterised using macros.

TEST_CASE("Blocking pipe pair can send and receive data", "[pipe][blocking]")
{
	using namespace blocking;
	using namespace std::chrono_literals;

	auto const [r, w] = create_pipe();

	auto const timeout = GENERATE(deadline::instant(), deadline(1ms));

	unsigned char r_buffer[2] = {};
	REQUIRE_THROWS_MATCHES(
		r.read_some(as_read_buffer(r_buffer, 1), timeout),
		std::system_error,
		match_error(std::errc::timed_out));
	
	unsigned char w_buffer = 42;
	REQUIRE(w.write_some(as_write_buffer(&w_buffer, 1)) == 1);

	REQUIRE(r.read_some(as_read_buffer(r_buffer, 2)) == 1);
	REQUIRE(r_buffer[0] == 42);
}

TEST_CASE("Blocking pipe pair can read data greedily", "[pipe][blocking]")
{
	using namespace blocking;
	using namespace std::chrono_literals;

	auto const [r, w] = create_pipe();

	unsigned char w_buffer = 1;
	REQUIRE(w.write_some(as_write_buffer(&w_buffer, 1)) == 1);

	unsigned char r_buffer[2] = {};
	vsm::atomic<bool> r_completed = false;

	auto r_future = test::spawn([&]()
	{
		r.read(as_read_buffer(r_buffer, 2));
		r_completed.store(true, std::memory_order_release);
	});

	// Wait for the other thread to hopefully start the I/O and suspend.
	std::this_thread::sleep_for(10ms);
	REQUIRE(!r_completed.load(std::memory_order_acquire));

	w_buffer = 2;
	REQUIRE(w.write_some(as_write_buffer(&w_buffer, 1)) == 1);

	r_future.get();
	REQUIRE(r_buffer[0] == 1);
	REQUIRE(r_buffer[1] == 2);
}

TEST_CASE("Blocking pipe pair partial greedy read fails", "[pipe][blocking]")
{
	using namespace blocking;
	using namespace std::chrono_literals;

	auto [r, w] = create_pipe();

	unsigned char w_buffer = 1;
	REQUIRE(w.write_some(as_write_buffer(&w_buffer, 1)) == 1);

	auto const timeout = GENERATE(deadline::never(), deadline(1ms));

	unsigned char r_buffer[2] = {};
	auto r_future = test::spawn([&]()
	{
		r.read(as_read_buffer(r_buffer, 2), timeout);
	});

	if (timeout == deadline::never())
	{
		w.close();
	}

	auto const expected_error = timeout != deadline::never()
		? std::errc::timed_out
		: std::errc::broken_pipe;

	REQUIRE_THROWS_MATCHES(
		r_future.get(),
		std::system_error,
		match_error(expected_error));
}

TEST_CASE("Asynchronous pipe pair can send and receive data", "[pipe][senders][async]")
{
	using namespace senders;

	auto multiplexer = default_multiplexer::create().value();

	sync_wait(multiplexer, []() -> task<void>
	{
		auto const [r, w] = co_await create_pipe();

		co_await ex::when_all
		(
			[&]() -> task<void>
			{
				unsigned char r_buffer = 0;
				REQUIRE(co_await r.read_some(as_read_buffer(&r_buffer, 1)) == 1);
				REQUIRE(r_buffer == 1);

				REQUIRE(co_await r.read_some(as_read_buffer(&r_buffer, 1)) == 1);
				REQUIRE(r_buffer == 2);
			}(),

			[&]() -> task<void>
			{
				unsigned char w_buffer[2] = { 1, 2 };
				REQUIRE(co_await w.write_some(as_write_buffer(w_buffer, 2)) == 2);
			}()
		);
	}());
}

} // namespace
