#pragma once

#include <allio/win32/detail/wsa.hpp>

#include <allio/impl/posix/socket.hpp>

#include <vsm/result.hpp>

#include <algorithm>
#include <limits>

namespace allio::win32 {

inline vsm::result<void> transform_wsa_buffers(untyped_buffers const buffers, WSABUF* const wsa_buffers)
{
	bool size_out_of_range = false;

	std::transform(
		buffers.begin(),
		buffers.end(),
		wsa_buffers,
		[&](untyped_buffer const buffer) -> WSABUF
		{
			if (buffer.size() > std::numeric_limits<ULONG>::max())
			{
				size_out_of_range = true;
			}

			return WSABUF
			{
				.len = static_cast<ULONG>(buffer.size()),
				.buf = static_cast<CHAR*>(const_cast<void*>(buffer.data())),
			};
		}
	);

	if (size_out_of_range)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	return {};
}

template<size_t StorageSize>
vsm::result<std::span<WSABUF>> make_wsa_buffers(
	detail::_wsa_buffers_storage<StorageSize>& storage,
	untyped_buffers const buffers)
{
	vsm_try(wsa_buffers, storage.reserve(buffers.size()));
	vsm_try_void(transform_wsa_buffers(buffers, wsa_buffers));
	return std::span<WSABUF>(wsa_buffers, buffers.size());
}

} // namespace allio::win32
