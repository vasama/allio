#include <allio/impl/win32/memory.hpp>

#include <allio/error.hpp>
#include <allio/impl/bounded_vector.hpp>

using namespace allio;
using namespace allio::win32;
using namespace allio::detail;

namespace {

struct system_info
{
	size_t page_size;
	size_t allocation_granularity;
};

} // namespace

static system_info const& get_system_info()
{
	static auto const value = []() -> system_info
	{
		SYSTEM_INFO system_info;
		GetSystemInfo(&system_info);

		return
		{
			.page_size = system_info.dwPageSize,
			.allocation_granularity = system_info.dwAllocationGranularity,
		};
	}();
	return value;
}


page_level detail::get_default_page_level()
{
	return get_page_level(get_system_info().page_size);
}

std::span<page_level const> detail::get_supported_page_levels()
{
	using supported_page_levels = bounded_vector<page_level, 2>;

	static auto const value = []() -> supported_page_levels
	{
		supported_page_levels levels;

		levels.try_push_back(get_default_page_level());

		if (size_t const large_page_size = GetLargePageMinimum())
		{
			levels.push_back(get_page_level(large_page_size));
			vsm_assert(levels[0] < levels[1]);
		}

		return levels;
	}();
	return value;
}

size_t detail::get_allocation_granularity(page_level const level)
{
	return std::max(get_system_info().allocation_granularity, get_page_size(level));
}


vsm::result<ULONG> win32::get_page_protection(protection const protection)
{
	switch (protection)
	{
		vsm_msvc_warning(push)
		vsm_msvc_warning(disable: 4063) // Disable C4063: Case is not a valid value for switch of enum.

		vsm_clang_diagnostic(push)
		vsm_clang_diagnostic(ignored "-Wswitch")

	case protection::none:
		return PAGE_NOACCESS;

	case protection::read:
		return PAGE_READONLY;

	case protection::read | protection::write:
		return PAGE_READWRITE;

	case protection::execute:
		return PAGE_EXECUTE;

	case protection::execute | protection::read:
		return PAGE_EXECUTE_READ;

	case protection::execute | protection::read | protection::write:
		return PAGE_EXECUTE_READWRITE;

	default:
		return vsm::unexpected(error::unsupported_operation);

		vsm_msvc_warning(pop)
		vsm_clang_diagnostic(pop)
	}
}
