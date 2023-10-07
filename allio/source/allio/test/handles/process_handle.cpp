#include <allio/process_handle.hpp>

#include <catch2/catch_all.hpp>

#include <filesystem>
#include <random>
#include <string>
#include <vector>

using namespace allio;

TEST_CASE("The current process cannot be awaited", "[process_handle]")
{
	auto const& process = this_process::get_handle();

	auto const r = process.wait();
	REQUIRE(!r);
	REQUIRE(r.error() == error::process_is_current_process);
}

TEST_CASE("Child process", "[process_handle]")
{
	std::vector<std::string> arguments;
	std::vector<std::string> environment;
	path working_directory;

	process_exit_code expected_exit_code = EXIT_SUCCESS;
	std::string stdin_data;
	std::string expected_stdout;
	std::string expected_stderr;

	SECTION("exit code can be observed")
	{
		expected_exit_code = GENERATE(EXIT_SUCCESS, EXIT_FAILURE);
		arguments.push_back("exit=" + std::to_string(expected_exit_code));
	}

	if (0)
	SECTION("stdout can be redirected")
	{
		arguments.push_back("stdout=The quick brown fox");
		expected_stdout += "a quick brown fox\n";
	}

	if (0)
	SECTION("stderr can be redirected")
	{
		arguments.push_back("stderr=jumps over the lazy dog");
		expected_stderr += "jumps over the lazy dog\n";
	}

	if (0)
	SECTION("stdin can be redirected")
	{
		stdin_data = "Lorem ipsum dolor sit amet";
		arguments.push_back("stdin");
		expected_stdout += "Lorem ipsum dolor sit amet\n";
	}

	if (0)
	SECTION("environment variables can be assigned")
	{
		environment.push_back("allio_test_variable=42");
		arguments.push_back("getenv=allio_test_variable");
		expected_stdout += "42\n";
	}

	if (0)
	SECTION("working directory can be assigned")
	{
		working_directory = path(std::filesystem::temp_directory_path().string());
		arguments.push_back("getcwd=" + working_directory.string());
		expected_stdout += working_directory.string() + "\n";
	}

	CAPTURE(arguments);
	CAPTURE(environment);
	CAPTURE(working_directory.string());

	auto const process = launch_process(
		path_view(allio_detail_test_program),
		allio::command_line(arguments),
		allio::environment(environment),
		allio::working_directory(working_directory)).value();

	auto const exit_code = process.wait().value();
	REQUIRE(exit_code == expected_exit_code);
}
