#include <allio/blocking/map.hpp>

#include <allio/blocking/file.hpp>
#include <allio/blocking/section.hpp>
#include <allio/test/filesystem.hpp>
#include <allio/test/memory.hpp>

#include <catch2/catch_all.hpp>

#include <algorithm>

using namespace allio;
using namespace allio::blocking;

static constexpr size_t KiB = 1024;
static constexpr size_t MiB = 1024 * KiB;
static constexpr size_t GiB = 1024 * MiB;

#if vsm_word_32
static constexpr size_t large_reservation_size = 1 * GiB;
#else
static constexpr size_t TiB = 1024 * GiB;
static constexpr size_t large_reservation_size = 1 * TiB;
#endif

TEST_CASE("Anonymous mappings can reserve large amounts of address space", "[map]")
{
	auto const map = map_memory(large_reservation_size, initial_commit(false));

	auto const page_size = map.page_size();
	auto const page_0 = reinterpret_cast<char*>(map.base());
	auto const page_n = page_0 + (map.size() - page_size);

	map.commit(page_0, page_size);
	memset(page_0, 1, page_size);

	map.commit(page_n, page_size);
	memset(page_n, 2, page_size);

	REQUIRE(std::all_of(
		page_0,
		page_0 + page_size,
		[=](char const x) { return x == 1; }));

	REQUIRE(std::all_of(
		page_n,
		page_n + page_size,
		[=](char const x) { return x == 2; }));
}

TEST_CASE("The protection of anonymous mappings can be changed", "[map][hardware_exception]")
{
	auto const map = map_memory(1, protection::none);

	void* const base = map.base();
	size_t const size = map.size();

	REQUIRE(test::test_memory_protection(base, size) == protection::none);

	map.commit(base, size, protection::read);
	REQUIRE(test::test_memory_protection(base, size) == protection::read);

	auto const set_page_protection = [&](protection const protection) -> bool
	{
		try
		{
			map.commit(base, size, protection);
			return true;
		}
		catch (std::system_error const&)
		{
			return false;
		}
	};

	if (set_page_protection(protection::write))
	{
		// Write-only mappings are most likely not supported, but if it happens that they are, test
		// that they work correctly.
		REQUIRE(test::test_memory_protection(base, size) == protection::write);
	}

	map.commit(base, size, protection::read_write);
	REQUIRE(test::test_memory_protection(base, size) == protection::read_write);

	test::setup_executable_memory_test(base, size);
	protection execute_protection = protection::execute;

	// First try to set execute only, but in case that fails, try again with both read and execute
	// protection. Linux in particular does not support execute only page protection.
	if (!set_page_protection(execute_protection))
	{
		execute_protection |= protection::read;
		map.commit(base, size, execute_protection);
	}

	REQUIRE(test::test_executable_memory(base, size) == execute_protection);
}

TEST_CASE("Sections using backing files can be created and mapped", "[section_handle][map]")
{
	path const file_path = test::get_temp_path();
	auto const mode = GENERATE(file_mode::read, file_mode::read_write);

	test::write_file_content(file_path, "check");
	std::string expected_content = "check";
	{
		auto const file = open_file(file_path, mode);
		auto const section = create_section(file, 5);
		auto const map = map_section(section, 0, 5);

		REQUIRE(memcmp(map.base(), "check", 5) == 0);

		if (vsm::all_flags(mode, file_mode::write_data))
		{
			memcpy(map.base(), "write", 5);
			expected_content = "write";
		}
	}
	test::check_file_content(file_path, expected_content);
}

TEST_CASE("Entire files can be mapped directly by handle", "[map]")
{
	path const file_path = test::get_temp_path();
	auto const mode = GENERATE(file_mode::read, file_mode::read_write);

	test::write_file_content(file_path, "check");
	std::string expected_content = "check";
	{
		auto const file = open_file(file_path, mode);
		auto const map = map_file(file);

		REQUIRE(memcmp(map.base(), "check", 5) == 0);

		if (vsm::all_flags(mode, file_mode::write_data))
		{
			memcpy(map.base(), "write", 5);
			expected_content = "write";
		}
	}
	test::check_file_content(file_path, expected_content);
}

TEST_CASE("Entire files can be mapped directly by path", "[map]")
{
	path const file_path = test::get_temp_path();
	auto const mode = GENERATE(file_mode::read, file_mode::read_write);

	test::write_file_content(file_path, "check");
	std::string expected_content = "check";
	{
		auto const map = map_file(file_path);

		REQUIRE(memcmp(map.base(), "check", 5) == 0);

		if (vsm::all_flags(mode, file_mode::write_data))
		{
			memcpy(map.base(), "write", 5);
			expected_content = "write";
		}
	}
	test::check_file_content(file_path, expected_content);
}
