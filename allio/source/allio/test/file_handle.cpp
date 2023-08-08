#include <allio/file_handle_async.hpp>

#include <allio/test/filesystem.hpp>
#include <allio/test/multiplexer.hpp>
#include <allio/test/task.hpp>
#include <allio/sync_wait.hpp>

#include <catch2/catch_all.hpp>

#include <string>
#include <vector>

#include <cstring>

using namespace allio;

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
	sync_wait(*multiplexer, [&]() -> detail::execution::task<void>
	{
		file_handle file = co_await error_into_except(open_file_async(*multiplexer, file_path));

		char buffer[] = "trash";
		co_await error_into_except(file.read_at_async(0, as_read_buffer(buffer, 5)));
		REQUIRE(memcmp(buffer, "allio", 5) == 0);
	}());
}

TEST_CASE("file_handle::write_at_async", "[file_handle]")
{
	path const file_path = get_temp_file_path("allio-test-file");

	unique_multiplexer_ptr const multiplexer = generate_multiplexer(true);

	write_file_content(file_path, "trash");
	sync_wait(*multiplexer, [&]() -> detail::execution::task<void>
	{
		file_handle file = co_await error_into_except(open_file_async(*multiplexer, file_path,
		{
			.mode = file_mode::write,
		}));

		co_await error_into_except(file.write_at_async(0, as_write_buffer("allio", 5)));
	}());
	check_file_content(file_path, "allio");
}

TEST_CASE("file_handle::write_at with many vectors", "[file_handle]")
{
	//static constexpr size_t buffer_count = 0xFFFF;
	static constexpr size_t buffer_count = 0x1;

	std::string data_write_buffer;
	data_write_buffer.resize(buffer_count);

	std::string data_read_buffer;
	data_read_buffer.resize(buffer_count, '0');

	// Fill data_write_buffer with random data.
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
		.multiplexable = multiplexer != nullptr,
		.mode = file_mode::write,
		.creation = file_creation::replace_existing,
	}).value();

	REQUIRE(file.write_at(0, data_write_buffers).value() == buffer_count);
	REQUIRE(file.read_at(0, data_read_buffers).value() == buffer_count);
	REQUIRE((data_read_buffer == data_write_buffer));
}
