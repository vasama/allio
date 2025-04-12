#include <allio/blocking/process.hpp>

#include <allio/blocking/byte_stream_io.hpp>
#include <allio/blocking/event.hpp>
#include <allio/blocking/pipe.hpp>
#include <allio/blocking/serialization.hpp>
#include <allio/path.hpp>
#include <allio/test/filesystem.hpp>
#include <allio/test/main.hpp>
#include <allio/test/match_error.hpp>
#include <allio/test/spawn.hpp>

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

//TODO: This test case requires opening the current process with sufficient access rights for handle
//      duplication. There is currently no way to request such access rights.
#if 0
TEST_CASE(
	"Handles can be duplicated from the current process",
	"[process][this_process][serialization]")
{
	auto const process = blocking::this_process::open();

	auto const event_1 = blocking::event(manual_reset_event);
	auto const event_serialized = blocking::encode_handle<std::string>(event_1);

	auto const event_2 = process.duplicate_handle<blocking::event_handle>(event_serialized);

	REQUIRE_THROWS_MATCHES(
		event_2.wait(deadline::instant()),
		std::system_error,
		match_error(std::errc::timed_out));

	event_1.signal();
	event_2.wait(deadline::instant());
}
#endif

TEST_CASE("Child process can be created", "[process]")
{
	using namespace blocking;

	blocking::process_handle const process = create_process(
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

TEST_CASE("Handles can be inherited by a child process", "[process][serialization]")
{
	auto const child_process = [](std::string_view const serialized_event)
	{
		blocking::decode_handle<blocking::event_handle>(serialized_event).signal();
	};

	auto const event = blocking::event(manual_reset_event, inheritable);

	auto const child_args = test::make_child_args(
		child_process,
		blocking::encode_handle<std::string>(event));

	auto const process = blocking::create_process(
		test::get_child_executable_path(),
		inherit_handles,
		process_arguments(child_args));

	event.wait(std::chrono::seconds(1));

	REQUIRE(process.wait().get_exit_code() == EXIT_SUCCESS);
}

TEST_CASE("Handles can be duplicated from a child process", "[process][serialization]")
{
	auto const child_process = [](std::string_view const serialized_pipe)
	{
		auto pipe = blocking::decode_handle<blocking::pipe_handle>(serialized_pipe);

		auto const event = blocking::event(manual_reset_event);
		pipe.write(allio::as_write_buffer(blocking::encode_handle<std::string>(event)));
		pipe.close();

		event.wait(std::chrono::seconds(1));
	};

	auto pipe_pair = blocking::create_pipe(inheritable);

	auto const child_args = test::make_child_args(
		child_process,
		blocking::encode_handle<std::string>(pipe_pair.write_pipe));

	auto const process = blocking::create_process(
		test::get_child_executable_path(),
		inherit_handles,
		process_arguments(child_args));

	pipe_pair.write_pipe.close();

	auto const serialized_event = blocking::read_to_end<std::string>(pipe_pair.read_pipe);
	auto const event = process.duplicate_handle<blocking::event_handle>(serialized_event);

	event.signal();

	REQUIRE(process.wait().get_exit_code() == EXIT_SUCCESS);
}

//TODO: This test case won't currently work on Linux due to the inability to get the exit code of an
//      opened process handle.
#if vsm_os_win32
TEST_CASE("Child process can be waited for upon handle destruction", "[process]")
{
	auto const child_process = [](std::string_view const serialized_event)
	{
		blocking::decode_handle<blocking::event_handle>(serialized_event)
			.wait(std::chrono::seconds(1));
	};
	
	auto const local_event = blocking::event(manual_reset_event);
	auto const child_event = blocking::event(manual_reset_event, inheritable);

	auto const child_args = test::make_child_args(
		child_process,
		blocking::encode_handle<std::string>(child_event));

	auto process_1 = blocking::create_process(
		test::get_child_executable_path(),
		wait_on_close,
		inherit_handles,
		process_arguments(child_args));

	//TODO: Duplicate the handle instead:
	auto const process_2 = blocking::open_process(process_1.get_id());
	
	auto const future = test::spawn([&]()
	{
		// The local event - indicating that the process handle close has returned - should not be
		// be signaled before the child event has been signaled.
		REQUIRE_THROWS_MATCHES(
			local_event.wait(std::chrono::milliseconds(100)),
			std::system_error,
			match_error(std::errc::timed_out));

		child_event.signal();
	});

	process_1.close();
	local_event.signal();

	REQUIRE(process_2.wait().get_exit_code() == EXIT_SUCCESS);
}
#endif
