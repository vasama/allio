#include <allio/impl/win32/command_line.hpp>

#include <allio/error.hpp>

#include <catch2/catch_all.hpp>

#include <format>
#include <string>
#include <vector>

using namespace allio;
using namespace allio::win32;

TEST_CASE("process command line generation", "[command_line][win32]")
{
	std::string program;
	std::vector<std::string> arguments;

	std::error_code expected_error;
	std::wstring expected_command_line;

	std::optional<std::vector<std::string>> capture_arguments;

	SECTION("trivial strings are passed through unchanged")
	{
		program = "./some_program";
		expected_command_line += L"./some_program";

		arguments.push_back("argument");
		expected_command_line += L" argument";
	}

	SECTION("strings containing spaces are wrapped in quotes")
	{
		program = "C:/Program Files/Some/program.exe";
		expected_command_line += L"\"C:/Program Files/Some/program.exe\"";

		arguments.push_back("this string contains spaces");
		expected_command_line += L" \"this string contains spaces\"";
	}

	SECTION("quotes are escaped")
	{
		program = "./some_program";
		expected_command_line += L"./some_program";

		arguments.push_back("--argument=\"value\"");
		expected_command_line += L" --argument=\\\"value\\\"";
	}

	SECTION("backslashes are escaped")
	{
		program = "C:\\Windows\\System32\\cmd.exe";
		expected_command_line += L"C:\\\\Windows\\\\System32\\\\cmd.exe";

		arguments.push_back("\\some\\network\\share");
		expected_command_line += L" \\\\some\\\\network\\\\share";
	}

	SECTION("the maximum command line length is accepted")
	{
		program = "program";
		expected_command_line += L"program ";

		size_t const argument_size = 32'766 - expected_command_line.size();
		arguments.push_back(std::string(argument_size, 'x'));
		expected_command_line.append(argument_size, 'x');

		capture_arguments = { std::format("xxx... [{}]", argument_size) };
	}

	SECTION("exceeding the maximum command line length is rejected")
	{
		program = "program";
		expected_command_line += L"program ";

		size_t const argument_size = 32'766 - expected_command_line.size() + 1;
		arguments.push_back(std::string(argument_size, 'x'));
		expected_error = error::command_line_too_long;

		capture_arguments = { std::format("xxx... [{}]", argument_size) };
	}

	CAPTURE(program);
	CAPTURE(capture_arguments ? *capture_arguments : arguments);

	command_line_storage storage;
	auto const r = make_command_line(
		storage,
		program,
		arguments);

	if (expected_error)
	{
		REQUIRE(!r);
		REQUIRE(r.error() == expected_error);
	}
	else
	{
		REQUIRE(r.value() == expected_command_line);
	}
}
