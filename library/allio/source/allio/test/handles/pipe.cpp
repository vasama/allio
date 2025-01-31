#include <allio/blocking/pipe.hpp>
#include <allio/senders/pipe.hpp>

#include <allio/senders/sync_wait.hpp>
#include <allio/senders/task.hpp>
#include <allio/test/match_error.hpp>

#include <exec/async_scope.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
namespace ex = stdexec;

namespace {

TEST_CASE("Blocking pipe pair can send and receive data", "[pipe][blocking]")
{
	using namespace blocking;

	auto const [r, w] = create_pipe(non_blocking);

	unsigned char buffer = 0;
	REQUIRE_THROWS_MATCHES(
		r.read_some(as_read_buffer(&buffer, 1), deadline::instant()),
		std::system_error,
		match_error(std::errc::timed_out));

	buffer = 42;
	REQUIRE(w.write_some(as_write_buffer(&buffer, 1)) == 1);

	buffer = 0;
	REQUIRE(r.read_some(as_read_buffer(&buffer, 1)) == 1);
	REQUIRE(buffer == 42);
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
