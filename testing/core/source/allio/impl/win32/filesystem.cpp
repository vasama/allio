#include <allio/testing/filesystem.hpp>

#include <vsm/unique_resource.hpp>

#include <Windows.h>

using namespace allio;
using namespace allio::test;

namespace {

struct handle_deleter
{
	void operator()(HANDLE const handle) const
	{
		CloseHandle(handle);
	}
};

using unique_handle = vsm::unique_resource<HANDLE, handle_deleter>;

} // namespace

std::shared_ptr<void> test::lock_test_directory(std::filesystem::path const& path)
{
	HANDLE const h_file = CreateFileW(
		path.native().c_str(),
		GENERIC_READ | GENERIC_WRITE,
		/* dwShareMode: */ 0,
		/* lpSecurityAttributes: */ nullptr,
		/* dwCreationDisposition: */ OPEN_ALWAYS,
		/* dwFlagsAndAttributes: */ FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
		/* hTemplateFile: */ NULL);

	if (h_file == INVALID_HANDLE_VALUE)
	{
		return nullptr;
	}

	return std::make_shared<unique_handle>(h_file);
}
