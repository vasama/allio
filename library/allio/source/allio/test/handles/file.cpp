#include <allio/blocking/directory.hpp>
#include <allio/blocking/file.hpp>
#include <allio/blocking/filesystem.hpp>

#include <allio/filesystem.hpp>
#include <allio/test/filesystem.hpp>

#include <vsm/defer.hpp>
#include <vsm/numeric.hpp>
#include <vsm/standard/memory.hpp>

#include <catch2/catch_all.hpp>

#include <thread>

#include <cstring>

using namespace allio;

namespace {

static size_t count_files(std::filesystem::path const& path)
{
	return vsm::truncating(std::ranges::distance(std::filesystem::directory_iterator(path)));
}

template<typename T>
class aligned_allocator
{
	size_t m_min_alignment;

public:
	using value_type = T;

	aligned_allocator()
		: m_min_alignment(0)
	{
	}

	explicit aligned_allocator(size_t const min_alignment)
		: m_min_alignment(min_alignment)
	{
	}

	template<vsm::not_same_as<T> U>
	aligned_allocator(aligned_allocator<U> const& other)
		: m_min_alignment(other.min_alignment())
	{
	}

	[[nodiscard]] size_t min_alignment() const
	{
		return m_min_alignment;
	}

	[[nodiscard]] T* allocate(size_t const size) const
	{
		return static_cast<T*>(operator new(size * sizeof(T), get_alignment()));
	}

	void deallocate(T* const storage, size_t const size) const
	{
		operator delete(storage, size * sizeof(T), get_alignment());
	}

private:
	[[nodiscard]] std::align_val_t get_alignment() const
	{
		return std::align_val_t(std::max(m_min_alignment, alignof(T)));
	}
};

static void random_fill(std::span<char> const string)
{
	auto& engine = Catch::sharedRng();
	std::uniform_int_distribution<unsigned> distribution(0, 15);

	std::ranges::generate(
		string,
		[&]()
		{
			return "0123456789ABCDEF"[distribution(engine)];
		});
}

} // namespace

TEST_CASE("Files can be read", "[file][blocking]")
{
	using namespace blocking;

	auto const path = test::get_temp_path();

	test::write_file_content(path, "allio");
	{
		auto const file = open_file(path, file_mode::read);

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

TEST_CASE("File can be linked into the filesystem", "[file][blocking]")
{
	using namespace blocking;

	auto const stdfs_path = test::get_temp_stdfs_path();
	vsm_assert(std::filesystem::create_directory(stdfs_path));

	auto const path_1 = allio::path((stdfs_path / "1").string());
	auto const path_2 = allio::path((stdfs_path / "2").string());
	{
		auto const file = open_file(path_1, file_opening::create_only);
		REQUIRE(count_files(stdfs_path) == 1);

		link_at(file, path_2);
		REQUIRE(count_files(stdfs_path) == 2);
	}
	REQUIRE(count_files(stdfs_path) == 2);

	test::check_file_content(path_1, "");
	test::check_file_content(path_2, "");

	test::write_file_content(path_1, "hello");
	test::check_file_content(path_2, "hello");
}

TEST_CASE("Anonymous files can be created and linked", "[file][blocking]")
{
	using namespace blocking;

	auto const stdfs_path = test::get_temp_stdfs_path();
	vsm_assert(std::filesystem::create_directory(stdfs_path));
	vsm_assert(count_files(stdfs_path) == 0);

	auto const base_path = allio::path(stdfs_path.string());
	auto const link_path = allio::path((stdfs_path / "link").string());
	{
		auto const file = open_anonymous_file(base_path);
		file.write(0, as_write_buffer(std::string_view("hello")));
		link_at(file, link_path);
	}
	REQUIRE(count_files(stdfs_path) == 1);

	test::check_file_content(link_path, "hello");
}

TEST_CASE("Files can be created with unique names", "[file][blocking]")
{
	using namespace blocking;

	auto const stdfs_path = test::get_temp_stdfs_path();
	vsm_assert(std::filesystem::create_directory(stdfs_path));
	vsm_assert(count_files(stdfs_path) == 0);

	auto const path = allio::path(stdfs_path.string());
	{
		auto const file_1 = open_unique_file(path);
		REQUIRE(count_files(stdfs_path) == 1);

		auto const file_2 = open_unique_file(path);
		REQUIRE(count_files(stdfs_path) == 2);
	}
	REQUIRE(count_files(stdfs_path) == 2);
}

TEST_CASE("File information can be queried", "[file][blocking]")
{
	using namespace blocking;
	using namespace std::chrono_literals;

	auto const path = test::get_temp_path();

	fs_time_point const t0 = fs_clock::now();
	std::this_thread::sleep_for(10ms);
	{
		test::write_file_content(path, "trash");
	}
	std::this_thread::sleep_for(10ms);

	fs_time_point const t1 = fs_clock::now();
	std::this_thread::sleep_for(10ms);
	{
		FILE* const file = fopen(path.string().c_str(), "w+");
		REQUIRE(file != nullptr);
		REQUIRE(fwrite("hello", 5, 1, file) == 1);
		fclose(file);
	}
	std::this_thread::sleep_for(10ms);

	fs_time_point const t2 = fs_clock::now();
	std::this_thread::sleep_for(10ms);
	{
		test::check_file_content(path, "hello");
	}
	std::this_thread::sleep_for(10ms);

	fs_time_point const t3 = fs_clock::now();
	std::this_thread::sleep_for(10ms);
	{
		using enum std::filesystem::perms;
		std::filesystem::permissions(
			path.string(),
			owner_write | group_write | others_write,
			std::filesystem::perm_options::remove);
	}
	std::this_thread::sleep_for(10ms);

	fs_time_point const t4 = fs_clock::now();

	fs_info_mask const requested_mask = fs_info_mask::standard | fs_info_mask::create_time;

	fs_info_buffer info_buffer;
	auto const info = query_information(path, info_buffer, requested_mask);
	REQUIRE(info.contains(fs_info_mask::standard));

	REQUIRE(info.type() == fs_type::regular);

	if (info.contains(fs_info_mask::create_time))
	{
		REQUIRE(t0 <= info.create_time());
		REQUIRE(info.create_time() <= t1);
	}

	REQUIRE(t1 <= info.data_change_time());
	REQUIRE(info.data_change_time() <= t2);

	REQUIRE(t2 <= info.data_access_time());
	REQUIRE(info.data_access_time() <= t3);

	REQUIRE(t3 <= info.attr_change_time());
	REQUIRE(info.attr_change_time() <= t4);
}

TEST_CASE("File can be written using direct I/O", "[file][blocking]")
{
	using namespace blocking;

	auto const path = test::get_temp_path();

	std::optional<std::basic_string<char, std::char_traits<char>, aligned_allocator<char>>> data;
	{
		auto const file = open_file(path, file_caching::none);
		auto const limits = query_information<fs_io_limits>(path);

		data.emplace(aligned_allocator<char>(limits.minimum_buffer_alignment));
		data->resize(limits.maximum_writing_alignment * 3, ' ');
		vsm_assert(vsm::memalignment(data->data()) >= limits.minimum_buffer_alignment);

		auto const write_region = [&](size_t const offset, size_t const size)
		{
			random_fill(std::span(data->data() + offset, size));
			file.write(offset, as_write_buffer(data->data() + offset, size));
		};

		write_region(0, data->size());
		write_region(limits.minimum_writing_alignment, limits.minimum_writing_alignment);
		write_region(limits.maximum_writing_alignment, limits.maximum_writing_alignment);
	}
	test::check_file_content(path, *data);
}

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
