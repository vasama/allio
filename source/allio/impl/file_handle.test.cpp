#include <allio/file_handle_async.hpp>

#include <allio/default_multiplexer.hpp>
#include <allio/sync_wait.hpp>

#include <unifex/task.hpp>

#include <catch2/catch_all.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include <cstdio>
#include <cstring>

using namespace allio;

namespace {

struct file_deleter
{
	void operator()(FILE* const file) const
	{
		fclose(file);
	}
};
using unique_file = std::unique_ptr<FILE, file_deleter>;

} // namespace

static path get_temp_file_path(std::string_view const filename)
{
	return path((std::filesystem::temp_directory_path() / filename).string());
}

static unique_file open_file(path const& path, bool const write = false)
{
	FILE* const file = fopen(path.string().c_str(), write ? "wb" : "rb");
	REQUIRE(file != nullptr);
	return unique_file(file);
}

static void check_file_content(path const& path, std::string_view const content)
{
	auto const file = open_file(path);

	std::string buffer;
	buffer.resize(content.size());

	REQUIRE(fread(buffer.data(), content.size(), 1, file.get()) == 1);
	REQUIRE(buffer == content);
}

static void write_file_content(path const& path, std::string_view const content)
{
	auto const file = open_file(path, true);
	REQUIRE(fwrite(content.data(), content.size(), 1, file.get()) == 1);
}

static void maybe_set_multiplexer(unique_multiplexer_ptr const& multiplexer, auto& handle)
{
	if (GENERATE(0, 1))
	{
		CAPTURE("Bound multiplexer");
		handle.set_multiplexer(multiplexer.get()).value();
	}
}

static unique_multiplexer_ptr generate_multiplexer(bool const required = false)
{
	if (required || GENERATE(0, 1))
	{
		return create_default_multiplexer().value();
	}
	return nullptr;
}

TEST_CASE("file_handle::read_at", "[file_handle]")
{
	path const file_path = get_temp_file_path("allio-test-file");

	unique_multiplexer_ptr const multiplexer = generate_multiplexer();
	bool const multiplexable = multiplexer != nullptr;

	write_file_content(file_path, "allio");
	{
		file_handle file;
		file.set_multiplexer(multiplexer.get()).value();
		file.open(file_path, { .multiplexable = multiplexable }).value();

		char buffer[] = "trash";
		REQUIRE(file.read_at(0, as_read_buffer(buffer, 5)).value() == 5);
		REQUIRE(memcmp(buffer, "allio", 5) == 0);
	}
}

TEST_CASE("file_handle::write_at", "[file_handle]")
{
	path const file_path = get_temp_file_path("allio-test-file");

	unique_multiplexer_ptr const multiplexer = generate_multiplexer();
	bool const multiplexable = multiplexer != nullptr;

	write_file_content(file_path, "trash");
	{
		file_handle file;
		file.set_multiplexer(multiplexer.get()).value();
		file.open(file_path, { .multiplexable = multiplexable, .mode = file_mode::write }).value();

		REQUIRE(file.write_at(0, as_write_buffer("allio", 5)).value() == 5);
	}
	check_file_content(file_path, "allio");
}

TEST_CASE("file_handle::read_at_async", "[file_handle]")
{
	path const file_path = get_temp_file_path("allio-test-file");

	unique_multiplexer_ptr const multiplexer = generate_multiplexer(true);

	write_file_content(file_path, "allio");
	sync_wait(*multiplexer, [&]() -> unifex::task<void>
	{
		file_handle file = co_await open_file_async(*multiplexer, file_path);

		char buffer[] = "trash";
		co_await file.read_at_async(0, as_read_buffer(buffer, 5));
		REQUIRE(memcmp(buffer, "allio", 5) == 0);
	}());
}

TEST_CASE("file_handle::write_at_async", "[file_handle]")
{
	path const file_path = get_temp_file_path("allio-test-file");

	unique_multiplexer_ptr const multiplexer = generate_multiplexer(true);

	write_file_content(file_path, "trash");
	sync_wait(*multiplexer, [&]() -> unifex::task<void>
	{
		file_handle file = co_await open_file_async(*multiplexer, file_path, { .mode = file_mode::write });

		co_await file.write_at_async(0, as_write_buffer("allio", 5));
	}());
	check_file_content(file_path, "allio");
}

TEST_CASE("file_handle::write_at with many vectors", "[file_handle]")
{
	static constexpr size_t buffer_count = 0xFFFF;

	std::string data_write_buffer;
	data_write_buffer.resize(buffer_count);

	std::string data_read_buffer;
	data_read_buffer.resize(buffer_count, '0');

	{
		auto& rng = Catch::sharedRng();
		std::uniform_int_distribution<int> distribution('A', 'Z');
		std::generate_n(data_write_buffer.data(), buffer_count, [&]()
		{
			return static_cast<char>(distribution(rng));
		});
	}

	std::vector<write_buffer> data_write_buffers;
	data_write_buffers.resize(buffer_count);

	std::vector<read_buffer> data_read_buffers;
	data_read_buffers.resize(buffer_count);

	for (size_t i = 0; i < buffer_count; ++i)
	{
		data_write_buffers[i] = as_write_buffer(&data_write_buffer[i], 1);
		data_read_buffers[i] = as_read_buffer(&data_read_buffer[i], 1);
	}


	path const file_path = get_temp_file_path("allio-test-file");

	unique_multiplexer_ptr const multiplexer = generate_multiplexer(true);

	file_handle file;
	file.set_multiplexer(multiplexer.get()).value();
	file.open(file_path,
	{
		.multiplexable = true,
		.mode = file_mode::write,
		.creation = file_creation::replace_existing,
	}).value();

	REQUIRE(file.write_at(0, data_write_buffers).value() == buffer_count);
	REQUIRE(file.read_at(0, data_read_buffers).value() == buffer_count);
	REQUIRE((data_read_buffer == data_write_buffer));
}
