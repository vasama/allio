#include <allio/blocking/directory.hpp>
#include <allio/senders/directory.hpp>

#include <allio/fs_object.hpp>
#include <allio/sync_wait.hpp>
#include <allio/task.hpp>
#include <allio/test/filesystem.hpp>

#include <vsm/defer.hpp>

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

TEST_CASE("Directory current path can be read", "[directory][blocking]")
{
	using namespace blocking;

	path_kind const kind = GENERATE(
		path_kind::any
		, path_kind::windows_nt
		, path_kind::windows_volume_guid
		, path_kind::windows_dos
	);

	auto const temp_path = test::get_temp_path();
	REQUIRE(std::filesystem::create_directory(temp_path.string()));

	auto const directory = open_directory(temp_path);
	auto const current_path = directory.get_current_path(kind);

	REQUIRE(std::filesystem::equivalent(
		temp_path.string(),
		current_path.string()));
}

TEST_CASE("Directory entries can be read", "[directory][blocking]")
{
	using namespace blocking;

	static constexpr size_t file_count = 400;

	auto const path = test::get_temp_path();
	auto file_names = fill_directory(path, file_count);

	auto const directory = open_directory(path);

	SECTION("Directory read")
	{
		while (true)
		{
			stream_buffer buffer;
			auto const stream = directory.read(buffer);

			if (!stream)
			{
				break;
			}

			for (directory_entry const& entry : stream)
			{
				REQUIRE(file_names.erase(entry.get_name().value()));
			}
		}
	}

	SECTION("Directory iterator")
	{
		for (auto iterator = directory.iterate(); iterator.next();)
		{
			directory_entry const& entry = iterator.get();
			REQUIRE(file_names.erase(entry.get_name().value()));
		}
	}

	SECTION("Directory iterator range-for")
	{
		for (directory_entry const& entry : directory.iterate())
		{
			REQUIRE(file_names.erase(entry.get_name().value()));
		}
	}

	REQUIRE(file_names.empty());
}

#if 0
TEST_CASE("Directory entries can be read asynchronously", "[directory][async]")
{
	using namespace senders;

	static constexpr size_t file_count = 400;

	auto const path = test::get_temp_path();
	auto file_names = fill_directory(path, file_count);

	auto multiplexer = default_multiplexer::create().value();

	sync_wait(multiplexer, [&]() -> task<void>
	{
		auto const directory = co_await open_directory(path);

		while (true)
		{
			stream_buffer buffer;
			auto const stream = co_await directory.read(buffer);

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
	}());
}
#endif


TEST_CASE("Current directory", "[directory][this_process][blocking]")
{
	using namespace blocking;

	auto const current_path = std::filesystem::current_path();
	{
		SECTION("Current directory path can be read")
		{
			auto const path = this_process::get_current_directory();
			[[maybe_unused]] auto const p = std::filesystem::current_path();
			REQUIRE(path.string() == std::filesystem::current_path());
		}

		SECTION("Current directory can be assigned")
		{
			auto const temp_path = test::get_temp_path();
			std::filesystem::create_directory(temp_path.string());

			this_process::set_current_directory(temp_path);

			REQUIRE(std::filesystem::equivalent(
				temp_path.string(),
				std::filesystem::current_path()));

		}

		SECTION("Current directory can be opened")
		{
			auto const directory = this_process::open_current_directory();

			REQUIRE(std::filesystem::equivalent(
				current_path.string(),
				directory.get_current_path().string()));
		}
	}

	if (std::error_code e; std::filesystem::current_path(current_path, e), e)
	{
		std::abort();
	}
}
