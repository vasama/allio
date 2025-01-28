#include <allio/blocking/pipe.hpp>
#include <allio/blocking/process.hpp>

#include <allio/path.hpp>
#include <allio/test/filesystem.hpp>
#include <allio/test/match_error.hpp>

#include <catch2/catch_all.hpp>

#include <chrono>
#include <filesystem>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace allio;

TEST_CASE("Waiting on the current process returns an error", "[process][this_process]")
{
	auto const process = blocking::this_process::open();

	REQUIRE_THROWS_MATCHES(
		process.wait(),
		std::system_error,
		match_error(error::process_is_current_process));
}

TEST_CASE("Child process can be created", "[process]")
{
	using namespace blocking;

	auto const process = create_process(
		path_view(allio_detail_test_exe));

	REQUIRE(process.wait().get_exit_code() == EXIT_SUCCESS);
}

TEST_CASE("Child process can be created with arguments", "[process]")
{
	using namespace blocking;

	std::filesystem::remove(allio_detail_test_exe_output);
	{
		std::string_view const args[] = { "print=hello" };

		auto const process = create_process(
			path_view(allio_detail_test_exe),
			process_arguments(args));

		REQUIRE(process.wait().get_exit_code() == EXIT_SUCCESS);
	}
	test::check_file_content(allio_detail_test_exe_output, "hello\n");
}

TEST_CASE("Child process exit code can be observed", "[process]")
{
	using namespace blocking;

	process_exit_code const exit_code = GENERATE(EXIT_SUCCESS, EXIT_FAILURE);
	std::string const args[] = { std::format("exit={}", exit_code) };

	auto const process = create_process(
		path_view(allio_detail_test_exe),
		process_arguments(args));

	REQUIRE(process.wait().get_exit_code() == exit_code);
}

TEST_CASE("Child process working directory can be changed", "[process]")
{
	using namespace blocking;

	auto wdir = std::filesystem::temp_directory_path().string();

	if (path_view::is_separator(wdir.back()))
	{
		wdir.pop_back();
	}

	std::filesystem::remove(allio_detail_test_exe_output);
	{
		std::string const args[] = { std::format("getcwd") };

#if 0
		auto a0 = path_view(allio_detail_test_exe);
		auto a1 = process_arguments(args);

		auto a2_v = detail::fs_path(path_view(wdir));
		auto a2 = working_directory(a2_v);

		auto const process = create_process(
			a0,
			a1,
			a2);
#else
		auto const process = create_process(
			path_view(allio_detail_test_exe),
			process_arguments(args),
			working_directory(path_view(wdir)));
#endif

		REQUIRE(process.wait().get_exit_code() == EXIT_SUCCESS);
	}
	test::check_file_content(allio_detail_test_exe_output, wdir + '\n');
}

TEST_CASE("Child process environment can be changed", "[process]")
{
	using namespace blocking;

	std::filesystem::remove(allio_detail_test_exe_output);
	{
		std::string_view const env[] =
		{
			"allio_test_variable_e=",
			"allio_test_variable_n=42",
			"allio_test_variable_s=foo bar",
		};

		std::string_view const args[] =
		{
			"getenv=allio_test_variable_e",
			"getenv=allio_test_variable_n",
			"getenv=allio_test_variable_s",
		};

		auto const process = create_process(
			path_view(allio_detail_test_exe),
			process_environment(env),
			process_arguments(args));

		REQUIRE(process.wait().get_exit_code() == EXIT_SUCCESS);
	}
	test::check_file_content(
		allio_detail_test_exe_output,
		"\n"
		"42\n"
		"foo bar\n");
}

TEST_CASE("Child stdin can be redirected", "[process][pipe]")
{
	using namespace blocking;

	auto pipe = create_pipe(inheritable);

	static constexpr std::string_view data = "input data";

	REQUIRE(pipe.write_pipe.write_some(
		as_write_buffer(data.data(), data.size())) == data.size());

	pipe.write_pipe.close();

	std::filesystem::remove(allio_detail_test_exe_output);
	{
		std::string_view const args[] = { "echo" };

		auto const process = create_process(
			path_view(allio_detail_test_exe),
			process_arguments(args),
			redirect_stdin(pipe.read_pipe));

		REQUIRE(process.wait().get_exit_code() == EXIT_SUCCESS);
	}
	test::check_file_content(allio_detail_test_exe_output, "input data\n");
}

TEST_CASE("Child stdout can be redirected", "[process][pipe]")
{
	using namespace blocking;

	auto pipe = create_pipe(inheritable);

	static constexpr std::string_view input_data = "input data";
	std::string const args[] = { std::format("cout={}", input_data) };

	auto const process = create_process(
		path_view(allio_detail_test_exe),
		process_arguments(args),
		redirect_stdout(pipe.write_pipe));

	pipe.write_pipe.close();
	REQUIRE(process.wait().get_exit_code() == EXIT_SUCCESS);

	char output_data[input_data.size() + 1];

	size_t const output_size = pipe.read_pipe.read_some(
		as_read_buffer(output_data, std::size(output_data)));

	REQUIRE(std::string_view(output_data, output_size) == "input data\n");
}

TEST_CASE("Child stderr can be redirected", "[process][pipe]")
{
	using namespace blocking;

	auto pipe = create_pipe(inheritable);

	static constexpr std::string_view input_data = "input data";
	std::string const args[] = { std::format("cerr={}", input_data) };

	auto const process = create_process(
		path_view(allio_detail_test_exe),
		process_arguments(args),
		redirect_stderr(pipe.write_pipe));

	pipe.write_pipe.close();
	REQUIRE(process.wait().get_exit_code() == EXIT_SUCCESS);

	char output_data[input_data.size() + 1];

	size_t const output_size = pipe.read_pipe.read_some(
		as_read_buffer(output_data, std::size(output_data)));

	REQUIRE(std::string_view(output_data, output_size) == "input data\n");
}

TEST_CASE("Child process can be terminated", "[process]")
{
	using namespace blocking;

	std::string_view const args[] = { "sleep=10" };

	auto const process = create_process(
		path_view(allio_detail_test_exe),
		process_arguments(args));

	std::this_thread::sleep_for(std::chrono::seconds(1));

	process.terminate();

	// Requesting termination again is fine.
	process.terminate();

	REQUIRE(process.wait().get_exit_code() != EXIT_SUCCESS);

	// Requesting termination after the program has already terminated is fine.
	process.terminate();
}

//TODO: Add test for wait_on_close. Probably requires handle serialization support and a function
//      in the test tool to wait on a specified handle.
