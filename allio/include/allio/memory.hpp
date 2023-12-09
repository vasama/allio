#pragma once

#include <vsm/assert.h>
#include <vsm/flags.hpp>
#include <vsm/result.hpp>

#include <bit>
#include <span>

#include <cstdint>

namespace allio {

enum class protection : uint8_t
{
	none                                = 0,

	read                                = 1 << 0,
	write                               = 1 << 1,
	execute                             = 1 << 2,

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

inline page_level get_page_level(size_t const size)
{
	vsm_assert(size > 1 && (size & size - 1) == 0);
	return static_cast<page_level>(std::countr_zero(size));
}

inline size_t get_page_size(page_level const level)
{
	return static_cast<size_t>(1) << static_cast<uint8_t>(level);
}

/// @return The default paging level for the platform.
/// @note No additional privileges are required for 
page_level get_default_page_level();

/// @return Array of paging levels supported by the platform.
///         The array is non-empty and sorted in ascending order.
/// @note Support for a paging level does not guarantee that creating mappings at
///       such a paging level will succeed, as additional privileges may be required.
std::span<page_level const> get_supported_page_levels();


size_t get_allocation_granularity(page_level level);

} // namespace allio
