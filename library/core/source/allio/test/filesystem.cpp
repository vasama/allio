#include <allio/test/filesystem.hpp>

#include <allio/impl/unique_name.hpp>

#include <vsm/exceptions.hpp>

#include <atomic>
#include <format>
#include <random>

using namespace allio;
using namespace allio::test;

namespace {

static constexpr std::string_view test_prefix = "test_";
static constexpr std::string_view lock_prefix = "lock_";

static std::filesystem::path const& get_test_root_directory()
{
	static constexpr auto make_test_root_directory = []() -> std::filesystem::path
	{
		auto path = std::filesystem::temp_directory_path() / "allio-test-suite";
		std::filesystem::create_directory(path);
		return path;
	};

	static std::filesystem::path const path = make_test_root_directory();
	return path;
}

class test_identifier_generator
{
	std::minstd_rand m_engine;

public:
	test_identifier_generator()
		: m_engine(std::random_device()())
	{
	}

	[[nodiscard]] std::string operator()()
	{
		std::uniform_int_distribution<uint64_t> distribution;
		return std::format("{:016x}", distribution(m_engine));
	}
};

struct test_directory
{
	std::shared_ptr<void> lock;
	std::filesystem::path path;
};

static test_directory create_test_directory()
{
	auto const& root_path = get_test_root_directory();
	test_identifier_generator identifier_generator;

	for (size_t i = 0; i < 1000; ++i)
	{
		auto const identifier = identifier_generator();

		auto const test_name = std::string(test_prefix) + identifier;
		auto const lock_name = std::string(lock_prefix) + identifier;

		auto /***/ test_path = root_path / test_name;
		auto const lock_path = root_path / lock_name;

		if (auto lock = lock_test_directory(lock_path))
		{
			std::filesystem::remove_all(test_path);

			if (std::filesystem::create_directory(test_path))
			{
				return { vsm_move(lock), vsm_move(test_path) };
			}
		}
	}

	vsm_except_throw_or_terminate(std::runtime_error("Unable to create test directory."));
}

static void delete_test_directories()
{
	auto const& root_path = get_test_root_directory();

	for (auto const& entry : std::filesystem::directory_iterator(root_path))
	{
		if (entry.is_directory())
		{
			auto const test_path = entry.path();
			auto const test_name = test_path.filename().string();

			if (test_name.starts_with(test_prefix))
			{
				auto const identifier = test_name.substr(test_prefix.size());

				auto const lock_name = std::string(lock_prefix) + identifier;
				auto const lock_path = root_path / lock_name;

				if (auto const lock = lock_test_directory(lock_path))
				{
					std::filesystem::remove_all(test_path);
				}
			}
		}
	}
}

static std::filesystem::path const& get_test_directory()
{
	struct test_directory_raii : test_directory
	{
		test_directory_raii()
			: test_directory(create_test_directory())
		{
		}

		test_directory_raii(test_directory_raii const&) = delete;
		test_directory_raii& operator=(test_directory_raii const&) = delete;

		~test_directory_raii()
		{
			std::filesystem::remove_all(path);
			lock.reset();

			delete_test_directories();
		}
	};

	static test_directory_raii const directory;
	return directory.path;
}

} // namespace

std::filesystem::path test::get_temp_stdfs_path()
{
	static std::atomic<uint64_t> next_index = 0;
	auto const index = next_index.fetch_add(1, std::memory_order_acq_rel);
	return get_test_directory() / std::format("case_{}", index);
}

path test::get_temp_path()
{
	return path(get_temp_stdfs_path().string());
}
