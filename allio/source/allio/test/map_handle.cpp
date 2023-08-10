#include <allio/map_handle.hpp>

#include <allio/test/filesystem.hpp>
#include <allio/test/signal.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;

//TODO: Use signal handler to test read and write access.

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
	size_t const fill_char_offset = Catch::rng();
	char const fill_char_0 = 'A' + +fill_char_offset % fill_char_range;
	char const fill_char_1 = 'A' + ~fill_char_offset % fill_char_range;

	page_level const page_level = GENERATE_COPY(
		page_level::default_level,
		from_range(get_supported_page_levels()));

	map_handle map = map_anonymous(reservation_size,
	{
		.commit = false,
	}).value();

	REQUIRE(map.size() >= reservation_size);

	size_t const page_size = map.get_page_size();
	if (page_level != page_level::default_value)
	{
		REQUIRE(page_size == get_page_size(page_level));
	}

	char* const page_0 = reinterpret_cast<char*>(map.base());
	REQUIRE(page_0 != nullptr);
	char* const page_1 = base + page_size;

	map.commit(page_0, page_size).value();
	REQUIRE(map.base() == page_0);
	memset(page_0, fill_char_0, page_size);

	map.commit(page_1, page_size).value();
	REQUIRE(map.base() == page_0);
	memset(page_1, fill_char_1, page_size);

	REQUIRE(std::all(
		page_0,
		page_0 + page_size,
		[=](char const x) { return x == fill_char_0; }));

	REQUIRE(std::all(
		page_1,
		page_1 + page_size,
		[=](char const x) { return x == fill_char_1; }));
}

TEST_CASE("mapping page access", "[map_handle]")
{
	map_handle map = map_anonymous(1,
	{
		.access = page_access::read,
	}).value();

	unsigned char volatile* const page = reinterpret_cast<unsigned char*>(map.base());

	REQUIRE(*page == 0);

	REQUIRE(capture_signal(signal::access_violation, [&]()
	{
		*page = 1;
	}));

	REQUIRE(*page == 0);

	map.set_page_access(page, 1, page_access::read_write).value();

	*page = 1;

	REQUIRE(*page == 1);
}

TEST_CASE("file section and mapping", "[section_handle][map_handle]")
{
	path const file_path = get_temp_file_path("allio-test-file");

	bool const writable = GENERATE(0, 1);

	write_file_content(file_path, "check");
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
	check_file_content(file_path, writable ? "write" : "check");
}
