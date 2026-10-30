#pragma once

#include <allio/path.hpp>

#include <vsm/platform.h>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <random>
#include <string_view>

#include <cstdio>

namespace allio::test {

std::shared_ptr<void> lock_test_directory(std::filesystem::path const& path);


std::filesystem::path get_temp_stdfs_path();

path get_temp_path();


struct file_deleter
{
	void operator()(FILE* const file) const
	{
		fclose(file);
	}
};
using unique_file = std::unique_ptr<FILE, file_deleter>;

inline unique_file open_file(char const* const path, bool const write = false)
{
	FILE* const file = fopen(path, write ? "wb" : "rb");
	REQUIRE(file != nullptr);
	return unique_file(file);
}

inline unique_file open_file(std::filesystem::path const& path, bool const write = false)
{
	return open_file(path.string().c_str(), write);
}

inline unique_file open_file(path const& path, bool const write = false)
{
	return open_file(path.string().c_str(), write);
}

inline void touch_file(char const* const path)
{
	(void)open_file(path, true);
}

inline void touch_file(path const& path)
{
	touch_file(path.string().c_str());
}

inline void check_file_content(char const* const path, std::string_view const content)
{
	unique_file const file = open_file(path);

	if (!content.empty())
	{
		std::string buffer;
		buffer.resize(content.size());
		CHECK(fread(buffer.data(), 1, content.size(), file.get()) == content.size());
		REQUIRE(buffer == content);
	}

	long const pos = ftell(file.get());
	REQUIRE(pos != -1);

	REQUIRE(fseek(file.get(), 0, SEEK_END) == 0);

	long const end = ftell(file.get());
	REQUIRE(end != -1);

	REQUIRE(pos == end);
}

inline void check_file_content(path const& path, std::string_view const content)
{
	check_file_content(path.string().c_str(), content);
}

inline void write_file_content(char const* const path, std::string_view const content)
{
	unique_file const file = open_file(path, true);
	REQUIRE(fwrite(content.data(), content.size(), 1, file.get()) == 1);
}

inline void write_file_content(path const& path, std::string_view const content)
{
	write_file_content(path.string().c_str(), content);
}

} // namespace allio::test
