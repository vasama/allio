#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
#include <charconv>

using namespace std;

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

int main(int const argc, char const* const* const argv)
{
	int exit_code = EXIT_SUCCESS;

	if (argc > 0)
	{
		cout << argv[0] << endl;
	}

	if (argc > 1)
	{
		for (string_view const command : span(argv + 1, argc - 1))
		{
			/**/ if (auto const value = parse(command, "stdout"))
			{
				cout << *value << endl;
			}
			else if (auto const value = parse(command, "stderr"))
			{
				cerr << *value << endl;
			}
			else if (auto const value = parse(command, "stdin"))
			{
				copy(
					istreambuf_iterator<char>(cin),
					istreambuf_iterator<char>(),
					ostreambuf_iterator<char>(cout));
			}
			else if (auto const value = parse(command, "getenv"))
			{
				cout << getenv(value->data()) << endl;
			}
			else if (auto const value = parse(command, "getcwd"))
			{
				cout << filesystem::current_path() << endl;
			}
			else if (auto const value = parse(command, "exit"))
			{
				from_chars(value->data(), value->data() + value->size(), exit_code);
			}
		}
	}

	return exit_code;
}
