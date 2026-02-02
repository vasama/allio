#include <allio/blocking/directory.hpp>
#include <allio/blocking/file.hpp>

#include <allio/test/filesystem.hpp>

#include <catch2/catch_all.hpp>

#include <cstring>

using namespace allio;

TEST_CASE("Files can be read", "[file][blocking]")
{
	using namespace blocking;

	auto const path = test::get_temp_path();

	test::write_file_content(path, "allio");
	{
		auto const file = open_file(path);

		char data[] = "trash";
		REQUIRE(file.read_some(0, as_read_buffer(data, 5)) == 5);
		REQUIRE(memcmp(data, "allio", 5) == 0);
	}
}

TEST_CASE("Files can be written", "[file][blocking]")
{
	using namespace blocking;

	auto const path = test::get_temp_path();

	test::write_file_content(path, "trash");
	{
		auto const file = open_file(path);

		char data[] = "allio";
		REQUIRE(file.write_some(0, as_write_buffer(data, 5)) == 5);
	}
	test::check_file_content(path, "allio");
}

#if 0
TEST_CASE("File can be linked into the filesystem", "[file][blocking]")
{
	using namespace blocking;

	//auto const path = test::get_temp_path();
	auto const path_1 = allio::path_view("C:\\Users\\vasama\\Downloads\\test\\file_1");
	auto const path_2 = allio::path_view("C:\\Users\\vasama\\Downloads\\test\\file_2");

	auto const file = open_file(path_1, file_opening::create_only);
	detail::link_at(file, path_2).value();
}

TEST_CASE("File can be created with unique name", "[file][blocking]")
{
	using namespace blocking;

	//auto const path = test::get_temp_path();
	auto const path = allio::path_view("C:\\Users\\vasama\\Downloads\\test");
	auto const file = open_unique_file(path);
}

TEST_CASE("Anonymous file can be created", "[file][blocking]")
{
	using namespace blocking;

	//auto const path = test::get_temp_path();
	auto const path = allio::path_view("C:\\Users\\vasama\\Downloads\\test");
	auto const new_path = allio::path_view("C:\\Users\\vasama\\Downloads\\test\\file");

	{
		auto const file = open_anonymous_file(path);
		file.write(0, as_write_buffer(std::string_view("hello")));
		detail::link_at(file, new_path).value();
	}

	[[maybe_unused]] int x = 0;
}
#endif

#if 0
#include <allio/senders/sync_wait.hpp>
#include <allio/senders/task.hpp>

TEST_CASE("file::read_at", "[file]")
{
	path const file_path = test::get_temp_file_path("allio-test-file");

	//unique_multiplexer_ptr const multiplexer = test::generate_multiplexer();
	unique_multiplexer_ptr const multiplexer = create_default_multiplexer();
	bool const multiplexable = multiplexer != nullptr;

	test::write_file_content(file_path, "allio");
	{
		file file;
		file.set_multiplexer(multiplexer.get());
		file.open(file_path, { .multiplexable = multiplexable });

		char buffer[] = "trash";
		REQUIRE(file.read_at(0, as_read_buffer(buffer, 5)) == 5);
		REQUIRE(memcmp(buffer, "allio", 5) == 0);
	}
}

TEST_CASE("file::write_at", "[file]")
{
	path const file_path = test::get_temp_file_path("allio-test-file");

	unique_multiplexer_ptr const multiplexer = test::generate_multiplexer();
	bool const multiplexable = multiplexer != nullptr;

	test::write_file_content(file_path, "trash");
	{
		file file;
		file.set_multiplexer(multiplexer.get());
		file.open(file_path, { .multiplexable = multiplexable, .mode = file_mode::write });

		REQUIRE(file.write_at(0, as_write_buffer("allio", 5)) == 5);
	}
	test::check_file_content(file_path, "allio");
}

TEST_CASE("file::read_at_async", "[file]")
{
	path const file_path = test::get_temp_file_path("allio-test-file");

	unique_multiplexer_ptr const multiplexer = test::generate_multiplexer(true);

	test::write_file_content(file_path, "allio");
	sync_wait(*multiplexer, [&]() -> detail::execution::task<void>
	{
		file file = co_await error_into_except(open_file_async(*multiplexer, file_path));

		char buffer[] = "trash";
		co_await error_into_except(file.read_at_async(0, as_read_buffer(buffer, 5)));
		REQUIRE(memcmp(buffer, "allio", 5) == 0);
	}());
}

TEST_CASE("file::write_at_async", "[file]")
{
	path const file_path = test::get_temp_file_path("allio-test-file");

	unique_multiplexer_ptr const multiplexer = test::generate_multiplexer(true);

	test::write_file_content(file_path, "trash");
	sync_wait(*multiplexer, [&]() -> detail::execution::task<void>
	{
		file file = co_await error_into_except(open_file_async(*multiplexer, file_path,
		{
			.mode = file_mode::write,
		}));

		co_await error_into_except(file.write_at_async(0, as_write_buffer("allio", 5)));
	}());
	test::check_file_content(file_path, "allio");
}

TEST_CASE("file::write_at with many vectors", "[file]")
{
	static constexpr size_t buffer_count = 0xFFFF;

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


	path const file_path = test::get_temp_file_path("allio-test-file");

	unique_multiplexer_ptr const multiplexer = test::generate_multiplexer(true);

	file file;
	file.set_multiplexer(multiplexer.get());
	file.open(file_path,
	{
		.multiplexable = multiplexer != nullptr,
		.mode = file_mode::write,
		.creation = file_creation::replace_existing,
	});

	REQUIRE(file.write_at(0, data_write_buffers) == buffer_count);
	REQUIRE(file.read_at(0, data_read_buffers) == buffer_count);
	REQUIRE((data_read_buffer == data_write_buffer));
}
#endif
