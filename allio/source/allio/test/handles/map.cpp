#include <allio/map.hpp>

#include <allio/file.hpp>
#include <allio/section.hpp>
#include <allio/test/filesystem.hpp>
#include <allio/test/memory.hpp>

#include <catch2/catch_all.hpp>

#include <algorithm>

using namespace allio;

TEST_CASE("anonymous mapping", "[map_handle]")
{
	using namespace blocking;

	static constexpr size_t KiB = 1024;
	static constexpr size_t MiB = 1024 * KiB;
	static constexpr size_t GiB = 1024 * MiB;
	static constexpr size_t TiB = 1024 * GiB;

	#if vsm_word_32
	static constexpr size_t reservation_size = MiB;
	#endif

	#if vsm_word_64
	static constexpr size_t reservation_size = TiB;
	#endif

	static constexpr size_t fill_char_range = 'Z' - 'A';

	std::uniform_int_distribution<size_t> distribution(0, fill_char_range);
	size_t const fill_char_offset = distribution(Catch::sharedRng());

	char const fill_char_0 = 'A' + (+fill_char_offset % fill_char_range);
	char const fill_char_1 = 'A' + (~fill_char_offset % fill_char_range);

#if 0 //TODO: Query for large page privileges and test if available.
	std::optional<page_level> page_level = GENERATE(from_range(get_supported_page_levels()));
	if (*page_level == get_default_page_level() && GENERATE(1, 0))
	{
		page_level = std::nullopt;
	}
#else
	auto const page_level = get_default_page_level();
	size_t const page_size = get_page_size(page_level);
#endif

	auto const map = map_anonymous(
		reservation_size,
		initial_commit(false),
		page_level).value();

	REQUIRE(map.size() >= reservation_size);
	REQUIRE(map.size() % page_size == 0);
	REQUIRE(map.page_level() == page_level);
	REQUIRE(map.page_size() == page_size);

	char* const page_0 = reinterpret_cast<char*>(map.base());
	REQUIRE(page_0 != nullptr);
	char* const page_1 = page_0 + page_size;

	map.commit(page_0, page_size).value();
	REQUIRE(map.base() == page_0);
	memset(page_0, fill_char_0, page_size);

	map.commit(page_1, page_size).value();
	REQUIRE(map.base() == page_0);
	memset(page_1, fill_char_1, page_size);

	REQUIRE(std::all_of(
		page_0,
		page_0 + page_size,
		[=](char const x) { return x == fill_char_0; }));

	REQUIRE(std::all_of(
		page_1,
		page_1 + page_size,
		[=](char const x) { return x == fill_char_1; }));
}

TEST_CASE("map_handle page protection", "[map_handle][causes_faults]")
{
	using namespace blocking;

	auto const map = map_anonymous(1, protection::none).value();

	void* const base = map.base();
	size_t const size = map.size();

	REQUIRE(test::test_memory_protection(base, size) == protection::none);

	map.commit(base, size, protection::read).value();
	REQUIRE(test::test_memory_protection(base, size) == protection::read);

	// Write-only mappings are probably most likely not supported,
	// but if it happens that they are, test that they work correctly.
	if (map.commit(base, size, protection::write))
	{
		REQUIRE(test::test_memory_protection(base, size) == protection::write);
	}

	map.commit(base, size, protection::read_write).value();
	REQUIRE(test::test_memory_protection(base, size) == protection::read_write);
}

TEST_CASE("file section and mapping", "[section_handle][map_handle]")
{
	using namespace blocking;

	path const file_path = test::get_temp_path();
	//TODO: Make sure file_mode is found in namespace allio.
	auto const mode = GENERATE(file_mode::read, file_mode::read_write);

	test::write_file_content(file_path, "check");
	std::string expected_content = "check";
	{
		auto const file = open_file(file_path, mode).value();
		auto const section = create_section(file, 5).value();
		auto const map = map_section(section, 0, 5).value();

		REQUIRE(memcmp(map.base(), "check", 5) == 0);

		if (vsm::any_flags(mode, file_mode::write_data))
		{
			memcpy(map.base(), "write", 5);
			expected_content = "write";
		}
	}
	test::check_file_content(file_path, expected_content);
}
