#pragma once

#include <vsm/assert.h>
#include <vsm/flags.hpp>
#include <vsm/result.hpp>

#include <bit>
#include <span>

#include <cstdint>

namespace allio::detail {

enum class protection : uint8_t
{
	none                                = 1,

	read                                = 1 | 1 << 1,
	write                               = 1 | 1 << 2,
	execute                             = 1 | 1 << 3,

	read_write                          = read | write,
	all                                 = read | write | execute,
};
vsm_flag_enum(protection);

enum class page_level : uint8_t
{
	_4KiB                               = 12,
	_16KiB                              = 14,
	_64KiB                              = 16,
	_512KiB                             = 19,
	_1MiB                               = 20,
	_2MiB                               = 21,
	_8MiB                               = 23,
	_16MiB                              = 24,
	_32MiB                              = 25,
	_256MiB                             = 28,
	_512MiB                             = 29,
	_1GiB                               = 30,
	_2GiB                               = 31,
	_16GiB                              = 34,
};

[[nodiscard]] inline page_level get_page_level(size_t const size)
{
	vsm_assert(size > 1 && (size & size - 1) == 0);
	return static_cast<page_level>(std::countr_zero(size));
}

[[nodiscard]] inline size_t get_page_size(page_level const level)
{
	return static_cast<size_t>(1) << static_cast<uint8_t>(level);
}


/// @return Array of paging levels supported by the platform.
///         The array is non-empty and sorted in ascending order.
/// @note Support for a paging level does not guarantee that creating mappings at
///       such a paging level will succeed, as additional privileges may be required.
[[nodiscard]] std::span<page_level const> get_supported_page_levels();


/// @return The default paging level for the platform.
/// @note No additional privileges are required for mapping memory at this page level.
[[nodiscard]] page_level get_default_page_level();

/// @return The default paging size for the platform.
/// @note No additional privileges are required for mapping memory at this page size.
[[nodiscard]] inline size_t get_default_page_size()
{
	return get_page_size(get_default_page_level());
}


/// @return The allocation granularity at the specified page level.
/// @note The allocation granularity represents the alignment and minimum size of virtual address
///       space allocations. Note that memory may be committed at a lower granularity depending on
///       the selected page size.
[[nodiscard]] size_t get_allocation_granularity(page_level level);

/// @return The allocation granularity at the default page level.
[[nodiscard]] inline size_t get_default_allocation_granularity()
{
	return get_allocation_granularity(get_default_page_level());
}

} // namespace allio::detail
