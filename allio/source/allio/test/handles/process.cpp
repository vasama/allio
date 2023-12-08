#include <allio/process.hpp>

#include <allio/pipe.hpp>
#include <allio/path.hpp>
#include <allio/test/filesystem.hpp>

#include <catch2/catch_all.hpp>

#include <chrono>
#include <filesystem>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace allio;

TEST_CASE("Waiting on the current process returns an error", "[process_handle]")
{
	auto const& process = this_process::get_handle();

	auto const r = process.wait();
	REQUIRE(!r);
	REQUIRE(r.error() == error::process_is_current_process);
}

TEST_CASE("Child process", "[process_handle]")
{
	using namespace blocking;

	bool const inherit = GENERATE(false, true);

	std::vector<std::string> arg;
	std::vector<std::string> env;
	path dir;

	std::string output;

	std::optional<std::string> cin;
	std::optional<std::string> cout;
	std::optional<std::string> cerr;

	process_exit_code exit_code = EXIT_SUCCESS;

	SECTION("The exit code can be observed")
	{
		exit_code = GENERATE(EXIT_SUCCESS, EXIT_FAILURE);
		arg.push_back("exit=" + std::to_string(exit_code));
	}

	SECTION("The standard output stream can be redirected")
	{
		arg.push_back("cout=A quick brown fox");
		cout = "A quick brown fox\n";
	}

	SECTION("The standard error stream can be redirected")
	{
		arg.push_back("cerr=jumps over the lazy dog");
		cerr = "jumps over the lazy dog\n";
	}

	SECTION("The standard input stream can be redirected")
	{
		arg.push_back("echo");
		cin = "Lorem ipsum dolor sit amet";
		output = "Lorem ipsum dolor sit amet\n";
	}

	SECTION("Environment variables can be assigned")
	{
		arg.push_back("getenv=allio_test_variable");
		env.push_back("allio_test_variable=42");
		output = "42\n";
	}

	SECTION("The working directory can be assigned")
	{
		dir = path(std::filesystem::temp_directory_path().string());

		REQUIRE(!dir.empty());
		if (path_view::is_separator(dir.string().back()))
		{
			dir.string().pop_back();
		}

		arg.push_back("getcwd");
		output = dir.string() + '\n';
	}

	CAPTURE(arg);
	CAPTURE(env);
	CAPTURE(dir.string());
	CAPTURE(output);

	auto launch_args = detail::make_io_args<process_t, process_t::launch_t>(
		path_view(allio_detail_test_program))(
			command_line(arg),
			inherit_handles(inherit));

	if (!env.empty())
	{
		launch_args.environment = env;
	}

	if (!dir.empty())
	{
		launch_args.working_directory = dir;
	}

	pipe_pair cin_pipe;
	if (cin)
	{
		CAPTURE(*cin);
		cin_pipe = create_pipe().value();
		REQUIRE(cin_pipe.write_pipe.write_some(
			as_write_buffer(cin->data(), cin->size())).value() == cin->size());
		cin_pipe.write_pipe.close();
		launch_args.std_input = cin_pipe.read_pipe.native().platform_handle;
	}

	pipe_pair cout_pipe;
	if (cout)
	{
		cout_pipe = create_pipe().value();
		launch_args.std_output = cout_pipe.write_pipe.native().platform_handle;
	}

	pipe_pair cerr_pipe;
	if (cerr)
	{
		cerr_pipe = create_pipe().value();
		launch_args.std_error = cerr_pipe.write_pipe.native().platform_handle;
	}

	process_handle process;
	detail::blocking_io<process_t, process_t::launch_t>(process, launch_args).value();

	REQUIRE(process.wait().value() == exit_code);
	test::check_file_content(allio_detail_test_program_output, output);

	if (cout)
	{
		std::string data;
		data.resize(cout->size());
		data.resize(cout_pipe.read_pipe.read_some(
			as_read_buffer(data.data(), data.size())).value());
		REQUIRE(data == *cout);
	}

	if (cerr)
	{
		std::string data;
		data.resize(cerr->size());
		data.resize(cerr_pipe.read_pipe.read_some(
			as_read_buffer(data.data(), data.size())).value());
		REQUIRE(data == *cerr);
	}
}

TEST_CASE("Child process can be terminated", "[process_handle]")
{
	using namespace blocking;

	std::string_view const args[] = { "sleep=10" };

	auto const process = launch_process(
		path_view(allio_detail_test_program),
		command_line(args)).value();

	std::this_thread::sleep_for(std::chrono::seconds(1));

	process.terminate().value();

	// Requesting termination again is fine.
	process.terminate().value();

	REQUIRE(process.wait() != EXIT_SUCCESS);

	// Requesting termination after the program has already terminated is fine.
	process.terminate().value();
}
