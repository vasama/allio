#include <allio/directory.hpp>

#include <allio/sync_wait.hpp>
#include <allio/task.hpp>
#include <allio/test/filesystem.hpp>

#include <catch2/catch_all.hpp>

#include <filesystem>
#include <string>
#include <unordered_set>

using namespace allio;
namespace ex = stdexec;

static std::unordered_set<std::string> fill_directory(path_view const base_path, size_t const file_count)
{
	static constexpr size_t name_size = 20;
	static constexpr size_t rand_size = 10;

	std::string path_string(base_path.string());
	std::filesystem::create_directory(path_string);

	if (!path_view::is_separator(path_string.back()))
	{
		path_string += path_view::preferred_separator;
	}

	size_t const base_size = path_string.size();
	path_string.resize(base_size + name_size);

	char* const name = path_string.data() + base_size;
	char* const name_rand = std::fill_n(name, name_size - rand_size, '0');

	std::unordered_set<std::string> file_names;
	for (size_t i = 0; i < file_count; ++i)
	{
		snprintf(name_rand, rand_size + 1, "%d", static_cast<int>(i));
		test::touch_file(path_string.c_str());
		REQUIRE(file_names.insert(name).second);
	}
	return file_names;
}


using stream_buffer = std::array<std::byte, 4096>;

TEST_CASE("Directory entries can be read", "[directory_handle][blocking]")
{
	static constexpr size_t file_count = 400;

	auto const path = test::get_temp_path();
	auto file_names = fill_directory(path, file_count);

	auto const directory = open_directory(path);
	while (true)
	{
		stream_buffer buffer;
		auto const stream = directory.read(buffer);

		if (!stream)
		{
			break;
		}

		for (directory_entry const entry : stream)
		{
			REQUIRE(file_names.erase(entry.get_name().value()));
		}
	}
	REQUIRE(file_names.empty());
}

#if 0

//#include <allio/directory_handle_async.hpp>
#include <allio/directory.hpp>

#include <allio/default_multiplexer.hpp>
#include <allio/sync_wait.hpp>

#include <unifex/task.hpp>

#include <catch2/catch_all.hpp>

#include <filesystem>
#include <unordered_set>

using namespace allio;

static path get_temp_path(std::string_view const name)
{
	return path((std::filesystem::temp_directory_path() / name).string());
}

TEST_CASE("directory_handle::read", "[directory_handle]")
{
	static constexpr size_t file_count = 400;

	path const directory_path = get_temp_path("allio-test-directory");
	write_directory_content(directory_path, file_count);

	std::unordered_set<std::string> files;
	for (auto const& entry : std::filesystem::directory_iterator(directory_path.string()))
	{
		REQUIRE(files.insert(entry.path().filename().string()).second);
	}
	REQUIRE(files.size() == file_count);

	directory_handle directory = open_directory(directory_path);

	std::byte stream_buffer[4096];
	directory_stream stream(stream_buffer);

	while (stream.read(directory))
	{
		for (directory_entry const& entry : stream)
		{
			CHECK(files.erase(entry.get_name()));
		}
	}

	CHECK(files.empty());
}

#if 0
TEST_CASE("directory_stream_handle async", "[directory_stream_handle]")
{
	static constexpr size_t file_count = 400;

	path const directory_path = get_temp_path("allio-test-directory");
	write_directory_content(directory_path, file_count);

	unique_multiplexer_ptr const multiplexer = create_default_multiplexer();

	std::unordered_set<std::string> files;
	for (auto const& entry : std::filesystem::directory_iterator(directory_path.string()))
	{
		REQUIRE(files.insert(entry.path().filename().string()).second);
	}
	REQUIRE(files.size() == file_count);

	sync_wait(*multiplexer, [&]() -> unifex::task<void>
	{
		directory_stream_handle stream = co_await open_directory_stream_async(*multiplexer, directory_path);

		while (co_await stream.fetch_async())
		{
			for (auto const& entry : stream.entries())
			{
				REQUIRE(files.erase(stream.get_name(entry)));
			}
		}
	}());

	REQUIRE(files.empty());
}
#endif

#endif
