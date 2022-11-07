#pragma once

#include <allio/directory_handle_async.hpp>

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

static void write_directory_content(path_view const base_path, size_t const file_count)
{
	static constexpr size_t prefix_size = 190;
	static constexpr size_t suffix_max_size = 10;
	static constexpr size_t max_size = prefix_size + suffix_max_size;

	std::string path_string(base_path.string());
	std::filesystem::create_directory(path_string);

	path_string += "/";

	size_t const base_size = path_string.size();
	path_string.resize(base_size + max_size);

	char* path_suffix = path_string.data() + base_size;

	for (size_t i = 0; i < prefix_size; ++i)
	{
		*path_suffix++ = 'f';
	}

	for (size_t i = 0; i < file_count; ++i)
	{
		snprintf(path_suffix, suffix_max_size, "%d", static_cast<int>(i));
		FILE* const file = fopen(path_string.c_str(), "w");
		REQUIRE(file != nullptr);
		fclose(file);
	}
}

TEST_CASE("directory_stream_handle", "[directory_stream_handle]")
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

	directory_stream_handle stream = open_directory_stream(directory_path).value();

	while (stream.fetch().value())
	{
		for (auto const& entry : stream.entries())
		{
			CHECK(files.erase(stream.get_name(entry).value()));
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

	unique_multiplexer_ptr const multiplexer = create_default_multiplexer().value();

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
				REQUIRE(files.erase(stream.get_name(entry).value()));
			}
		}
	}());

	REQUIRE(files.empty());
}
#endif
