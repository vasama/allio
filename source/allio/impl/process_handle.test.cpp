#include <allio/process_handle.hpp>

#include <catch2/catch_all.hpp>

#include <random>

using namespace allio;

TEST_CASE("process_handle::current", "[process_handle]")
{
	process_handle current = process_handle::current();
	CHECK_FALSE(current.wait());
}

TEST_CASE("process_handle::launch", "[process_handle]")
{
	using namespace path_literals;

	auto& rng = Catch::sharedRng();
	std::uniform_int_distribution distribution(1, 1000);

	int const lhs = distribution(rng);
	int const rhs = distribution(rng);

	char lhs_buffer[32];
	sprintf(lhs_buffer, "%d", lhs);

	char rhs_buffer[32];
	sprintf(rhs_buffer, "%d", rhs);

	input_string_view const arguments[] =
	{
		input_string_view(std::string_view(lhs_buffer)),
		input_string_view(std::string_view(rhs_buffer)),
	};

	process_handle process = launch_process(
		allio_detail_ADD_NUMBERS_PATH ""_path,
		{ .arguments = arguments }
	).value();

	REQUIRE(process.wait().value() == lhs + rhs);
}
