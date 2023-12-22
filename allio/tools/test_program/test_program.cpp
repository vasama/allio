#include <algorithm>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
#include <thread>

#include <cstdio>

#if _WIN32
#include <fcntl.h>
#include <io.h>
#endif

using namespace std;
using namespace std::chrono;
using namespace std::filesystem;

static optional<string_view> parse(string_view argument, string_view const command)
{
	if (argument.starts_with(command))
	{
		argument.remove_prefix(command.size());

		if (argument.starts_with("="))
		{
			argument.remove_prefix(1);
		}

		return argument;
	}

	return nullopt;
}

static void set_binary_mode(FILE* const file)
{
#if _WIN32
	_setmode(_fileno(file), _O_BINARY);
#endif
}

int main(int const argc, char const* const* const argv)
{
	std::ofstream out(
		allio_detail_test_program_output,
		std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);

	int exit_code = EXIT_SUCCESS;

	if (out.is_open())
	{
		for (string_view const command : span(argv, argc).subspan(1))
		{
			/**/ if (auto const value = parse(command, "program"))
			{
				out << argv[0] << endl;
			}
			else if (auto const value = parse(command, "print"))
			{
				out << *value << endl;
			}
			else if (auto const value = parse(command, "echo"))
			{
				set_binary_mode(stdin);
				copy(
					istreambuf_iterator<char>(cin),
					istreambuf_iterator<char>(),
					ostreambuf_iterator<char>(out));
				out << endl;
			}
			else if (auto const value = parse(command, "cout"))
			{
				set_binary_mode(stdout);
				cout << *value << endl;
			}
			else if (auto const value = parse(command, "cerr"))
			{
				set_binary_mode(stderr);
				cerr << *value << endl;
			}
			else if (auto const value = parse(command, "getenv"))
			{
				out << getenv(value->data()) << endl;
			}
			else if (auto const value = parse(command, "getcwd"))
			{
				out << current_path().string() << endl;
			}
			else if (auto const value = parse(command, "exit"))
			{
				from_chars(value->data(), value->data() + value->size(), exit_code);
			}
			else if (auto const value = parse(command, "sleep"))
			{
				uint32_t seconds = 0;
				from_chars(value->data(), value->data() + value->size(), seconds);

				out << "sleeping for " << seconds << " seconds" << endl;
				if (seconds != 0)
				{
					auto const t = steady_clock::now() + chrono::seconds(seconds);
					do
					{
						this_thread::sleep_for(chrono::seconds(1));
					} while (steady_clock::now() < t);
				}

				exit_code = EXIT_FAILURE;
			}
		}
	}
	else
	{
		exit_code = EXIT_FAILURE;
	}

	return exit_code;
}
