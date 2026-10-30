#include <allio/testing/filesystem.hpp>

#include <vsm/unique_resource.hpp>
#include <vsm/utility.hpp>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace allio;
using namespace allio::test;

namespace {

struct fd_deleter
{
	void operator()(int const fd) const
	{
		close(fd);
	}
};

using unique_fd = vsm::unique_resource<int, fd_deleter>;


static unique_fd open_directory(std::filesystem::path const& path)
{
	int const fd = open(path.native().c_str(), O_DIRECTORY);

	if (fd == -1)
	{
		return vsm::null_resource;
	}

	return unique_fd(fd);
}

static unique_fd create_file_at(int const dir_fd, std::filesystem::path const& path)
{
	int const fd = openat(dir_fd, path.native().c_str(), O_CREAT | O_RDWR, /* mode: */ 0666);

	if (fd == -1)
	{
		return vsm::null_resource;
	}

	return unique_fd(fd);
}

static void try_unlink_file_at(int const dir_fd, std::filesystem::path const& path)
{
	(void)unlinkat(dir_fd, path.native().c_str(), /* flags: */ 0);
}

static bool is_linked(int const fd)
{
	struct stat st;
	return fstat(fd, &st) == 0 && st.st_nlink > 0;
}

static bool lock_file(int const fd)
{
	return flock(fd, LOCK_EX | LOCK_NB) == 0 && is_linked(fd);
}

} // namespace

std::shared_ptr<void> test::lock_test_directory(std::filesystem::path const& path)
{
	struct test_directory_lock
	{
		unique_fd m_root_fd;
		unique_fd m_lock_fd;
		std::filesystem::path m_lock_name;

		explicit test_directory_lock(
			unique_fd root_fd,
			unique_fd lock_fd,
			std::filesystem::path lock_name)
			: m_root_fd(vsm_move(root_fd))
			, m_lock_fd(vsm_move(lock_fd))
			, m_lock_name(vsm_move(lock_name))
		{
		}

		test_directory_lock(test_directory_lock const&) = delete;
		test_directory_lock& operator=(test_directory_lock const&) = delete;

		~test_directory_lock()
		{
			try_unlink_file_at(m_root_fd.get(), m_lock_name);
		}
	};

	auto const root_path = path.parent_path();
	auto const lock_name = path.filename();

	if (auto root_fd = open_directory(root_path))
	{
		if (auto lock_fd = create_file_at(root_fd.get(), lock_name))
		{
			if (lock_file(lock_fd.get()))
			{
				return std::make_shared<test_directory_lock>(
					vsm_move(root_fd),
					vsm_move(lock_fd),
					vsm_move(lock_name));
			}
		}
	}

	return nullptr;
}
