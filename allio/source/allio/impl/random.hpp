#pragma once

#include <allio/error.hpp>

#include <vsm/result.hpp>

#include <span>

#include <cstddef>

namespace allio::detail {

vsm::result<size_t> secure_random_fill_some(std::span<std::byte> buffer);

inline vsm::result<void> secure_random_fill(std::span<std::byte> const buffer)
{
	vsm_try(size, secure_random_fill_some(buffer));

	if (size != buffer.size())
	{
		return vsm::unexpected(error::unknown_failure);
	}

	return {};
}


} // namespace allio::detail
