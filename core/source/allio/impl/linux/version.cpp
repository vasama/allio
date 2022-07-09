#include <allio/impl/linux/version.hpp>

#include <optional>

#include <cstdio>

#include <sys/utsname.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

static std::optional<int> read_kernel_version()
{
	utsname data;

	if (uname(&data) == -1)
	{
		return std::nullopt;
	}

	int major;
	int minor;
	int patch;

	if (sscanf(data.release, "%d.%d.%d", &major, &minor, &patch) != 3)
	{
		return std::nullopt;
	}

	return KERNEL_VERSION(major, minor, patch);
}

int linux::get_kernel_version()
{
	static int const version = read_kernel_version().value_or(LINUX_VERSION_CODE);
	return version;
}
