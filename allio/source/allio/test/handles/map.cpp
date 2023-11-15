#include <allio/map.hpp>

#include <allio/test/filesystem.hpp>
#include <allio/test/signal.hpp>

#include <catch2/catch_all.hpp>

#include <algorithm>

using namespace allio;

TEST_CASE("anonymous mapping", "[map_handle]")
{
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
	auto const page_level = GENERATE(
		as<std::optional<allio::page_level>>(),
		std::nullopt,
		get_default_page_level());
#endif

	map_handle map;
	map.map(reservation_size,
	{
		.commit = false,
		.page_level = page_level,
	}).value();

#if 0
	map_handle map = map_anonymous(reservation_size,
	{
		.commit = false,
	}).value();
#endif

	REQUIRE(map.size() >= reservation_size);

	size_t const page_size = map.get_page_size();
	if (page_level)
	{
		REQUIRE(page_size == get_page_size(*page_level));
	}

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

TEST_CASE("map_handle page protection", "[map_handle]")
{
	map_handle map;
	map.map(1,
	{
		.protection = protection::read,
	}).value();

#if 0
	map_handle map = map_anonymous(1,
	{
		.protection = protection::read,
	}).value();
#endif

	unsigned char* const page = reinterpret_cast<unsigned char*>(map.base());
	unsigned char volatile& volatile_byte = *page;

	map.set_protection(page, 1, protection::read).value();
	REQUIRE(volatile_byte == 0);

	map.set_protection(page, 1, protection::read_write).value();
	REQUIRE(volatile_byte == 0);

	volatile_byte = 1;
	REQUIRE(volatile_byte == 1);

	map.set_protection(page, 1, protection::read).value();
	REQUIRE(volatile_byte == 1);
}

#if 0
TEST_CASE("file section and mapping", "[section_handle][map_handle]")
{
	path const file_path = test::get_temp_file_path("allio-test-file");

	bool const writable = GENERATE(0, 1);

	test::write_file_content(file_path, "check");
	{
		file_handle const file = open_file(file_path).value();
		section_handle const section = section_create(file, 5).value();
		map_handle const map = map_section(section, 0, 5).value();

		REQUIRE(map.size() >= 5);
		REQUIRE(memcmp(map.base(), "check", 5) == 0);

		if (writable)
		{
			memcpy(map.base(), "write", 5);
		}
	}
	test::check_file_content(file_path, writable ? "write" : "check");
}
#endif
