#pragma once

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>

#include <cstdio>

namespace allio::test {

struct file_deleter
{
	void operator()(FILE* const file) const
	{
		fclose(file);
	}
};
using unique_file = std::unique_ptr<FILE, file_deleter>;

inline path get_temp_file_path(std::string_view const filename)
{
	return path((std::filesystem::temp_directory_path() / filename).string());
}

inline unique_file open_file(path const& path, bool const write = false)
{
	FILE* const file = fopen(path.string().c_str(), write ? "wb" : "rb");
	REQUIRE(file != nullptr);
	return unique_file(file);
}

inline void check_file_content(path const& path, std::string_view const content)
{
	unique_file const file = open_file(path);

	std::string buffer;
	buffer.resize(content.size());

	REQUIRE(fread(buffer.data(), content.size(), 1, file.get()) == 1);
	REQUIRE(buffer == content);
}

inline void write_file_content(path const& path, std::string_view const content)
{
	unique_file const file = open_file(path, true);
	REQUIRE(fwrite(content.data(), content.size(), 1, file.get()) == 1);
}

} // namespace allio::test
