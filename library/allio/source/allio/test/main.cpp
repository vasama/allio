#include <allio/test/main.hpp>

#include <vsm/assert.h>

#include <catch2/catch_all.hpp>

#include <format>
#include <string_view>

#include <cstdio>

using namespace allio;
using namespace allio::test;
using namespace allio::test::detail;


static constexpr std::string_view entry_point_option = "--allio-entry-point=";

static uintptr_t entry_point_to_offset(child_entry_point_t const entry_point)
{
	uintptr_t const base_address = reinterpret_cast<uintptr_t>(entry_point_to_offset);
	return reinterpret_cast<uintptr_t>(entry_point) - base_address;
}

static child_entry_point_t offset_to_entry_point(uintptr_t const offset)
{
	uintptr_t const base_address = reinterpret_cast<uintptr_t>(entry_point_to_offset);
	return reinterpret_cast<child_entry_point_t>(base_address + offset);
}


static path_view this_executable_path;

path_view test::get_child_executable_path()
{
	return this_executable_path;
}

std::string test::get_entry_point_option(child_entry_point_t const entry_point)
{
	return std::format("{}{:08x}", entry_point_option, entry_point_to_offset(entry_point));
}


static child_entry_point_t get_entry_point(int& argc, char const* const*& argv)
{
	if (argc <= 1)
	{
		return nullptr;
	}

	std::string_view const option = argv[1];
	if (!option.starts_with(entry_point_option))
	{
		return nullptr;
	}

	std::string_view const option_value = option.substr(entry_point_option.size());
	char const* const value_beg = option_value.data();
	char const* const value_end = option_value.data() + option_value.size();

	uintptr_t entry_point_offset;
	auto const r = std::from_chars(value_beg, value_end, entry_point_offset, 0x10);

	if (r.ec != std::errc() || r.ptr != value_end)
	{
		return nullptr;
	}

	argc -= 2;
	argv += 2;

	return offset_to_entry_point(entry_point_offset);
}

int main(int argc, char const* const* argv)
{
	this_executable_path = path_view(argv[0]);

	if (!this_executable_path.is_absolute())
	{
		std::fprintf(stderr, "This test suite must be invoked using an absolute path.\n");
		return EXIT_FAILURE;
	}

	if (child_entry_point_t const entry_point = get_entry_point(argc, argv))
	{
		try
		{
			return entry_point(argc, argv);
		}
		catch (std::exception const& e)
		{
			std::fprintf(stderr, "%s\n", e.what());
		}

		return EXIT_FAILURE;
	}

	return Catch::Session().run(argc, argv);
}
