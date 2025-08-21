#include <allio/blocking/standard_stream.hpp>

#include <allio/blocking/byte_stream_io.hpp>
#include <allio/blocking/pipe.hpp>
#include <allio/blocking/process.hpp>
#include <allio/test/main.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;

namespace {

TEST_CASE("Standard streams can be read from and written to", "[standard_stream]")
{
	using namespace blocking;
	using namespace std::string_view_literals;

	auto i_pipe = create_pipe(inheritable);
	auto o_pipe = create_pipe(inheritable);
	auto e_pipe = create_pipe(inheritable);

	REQUIRE(i_pipe.write_pipe.write(as_write_buffer("hello world"sv)));

	auto const child_process = []()
	{
		auto const input = read_to_end<std::string>(cin);

		cout.write(as_write_buffer("out: "sv));
		cout.write(as_write_buffer(input));

		cerr.write(as_write_buffer("err: "sv));
		cerr.write(as_write_buffer(input));
	};

	auto const child_args = test::make_child_args(child_process);

	auto const process = create_process(
		test::get_child_executable_path(),
		process_arguments(child_args),
		redirect_stdin(i_pipe.read_pipe),
		redirect_stdout(o_pipe.write_pipe),
		redirect_stderr(e_pipe.write_pipe));

	i_pipe.write_pipe.close();
	o_pipe.write_pipe.close();
	e_pipe.write_pipe.close();

	REQUIRE(process.wait().get_exit_code() == EXIT_SUCCESS);

	REQUIRE(read_to_end<std::string>(o_pipe.read_pipe) == "out: hello world");
	REQUIRE(read_to_end<std::string>(e_pipe.read_pipe) == "err: hello world");
}

} // namespace
