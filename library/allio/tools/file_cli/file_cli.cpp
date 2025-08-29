#include <allio/blocking/directory.hpp>
#include <allio/blocking/file.hpp>

#include <print>

namespace io = allio::blocking;

static std::optional<char> interpret_escape(char const character)
{
	switch (character)
	{
	case '\'': return '\'';
	case '\"': return '\"';
	case '\\': return '\\';

	case '0': return '\0';
	case '?': return '\?';
	case 'a': return '\a';
	case 'b': return '\b';
	case 'f': return '\f';
	case 'n': return '\n';
	case 'r': return '\r';
	case 't': return '\t';
	case 'v': return '\v';

	default: return std::nullopt;
	}
}

static std::optional<char> parse_escape(
	std::string_view::iterator& pos,
	std::string_view::iterator const end)
{
	if (*pos != '\\')
	{
		return std::nullopt;
	}

	std::string_view::iterator const next = std::next(pos);

	if (next == end)
	{
		return std::nullopt;
	}

	auto const escape = interpret_escape(*next);

	if (escape)
	{
		pos = std::next(next);
	}

	return escape;
}

static std::string unescape(std::string_view const argument)
{
	std::string s;

	for (auto pos = argument.begin(), end = argument.end(); pos != end;)
	{
		if (auto const escape = parse_escape(pos, end))
		{
			s.push_back(*escape);
		}
		else
		{
			s.push_back(*pos++);
		}
	}

	return s;
}

static void main_impl(int const argc, char const* const* const argv)
{
	int argi = 0;

	auto const has_arg = [&]()
	{
		return argi < argc;
	};

	auto const consume_arg = [&]() -> std::string_view
	{
		if (argi == argc)
		{
			throw std::runtime_error("Expect command line argument");
		}

		return argv[argi++];
	};

	std::string_view const command = consume_arg();

	/**/ if (false)
	{
	}
	else if (command == "ls")
	{
		io::directory_handle dir;

		if (has_arg())
		{
			dir = io::open_directory(allio::path_view(unescape(consume_arg())));
		}
		else
		{
			dir = io::this_process::open_current_directory();
		}

		for (auto const& entry : dir.iterate())
		{
			std::print("{}\n", entry.get_name<std::string>());
		}
	}
	else if (command == "touch")
	{
		(void)io::open_file(
			allio::path_view(unescape(consume_arg())),
			allio::file_opening::open_or_create);
	}
#if 0
	else if (command == "rm")
	{
	}
#endif
	else
	{
		throw std::runtime_error(std::format("Unrecognized command: {}", command));
	}
}

int main(int const argc, char const* const* const argv)
{
	try
	{
		main_impl(argc - 1, argv + 1);
		return EXIT_SUCCESS;
	}
	catch (std::exception const& e)
	{
		std::print(stderr, "{}\n", std::string_view(e.what()));
	}
	return EXIT_FAILURE;
}
