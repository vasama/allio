#include <charconv>
#include <string_view>

#include <cstdio>

int main(int const argc, char const* const* const argv)
{
	if (argc != 3)
	{
		return -1;
	}

	std::string_view const lhs_string = argv[1];
	std::string_view const rhs_string = argv[2];

	auto const parse = [](std::string_view const s) -> int
	{
		char const* const beg = s.data();
		char const* const end = beg + s.size();

		int value;
		if (auto const r = std::from_chars(beg, end, value); r.ec != std::errc{} || r.ptr != end)
		{
			return -1;
		}
		return value;
	};

	int const lhs = parse(lhs_string);
	int const rhs = parse(rhs_string);

	if (lhs < 0 || rhs < 0)
	{
		return -1;
	}

	return lhs + rhs;
}
