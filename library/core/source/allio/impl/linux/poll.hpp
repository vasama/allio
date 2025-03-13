#pragma once

#include <allio/error.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/timeout.hpp>

#include <vsm/assert.h>
#include <vsm/result.hpp>

#include <poll.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

[[nodiscard]] inline vsm::result<short> poll(
	int const fd,
	short const events,
	deadline const deadline)
{
	vsm_assert(events != 0); //PRECONDITION

	pollfd poll_fd =
	{
		.fd = fd,
		.events = events,
	};

	int const r = ppoll(
		&poll_fd,
		/* count: */ 1,
		kernel_timeout<timespec>(deadline),
		/* sigmask: */ nullptr);

	if (r == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	if (r == 0)
	{
		return vsm::unexpected(allio_error(error::operation_timed_out));
	}

	vsm_assert((poll_fd.revents & events) != 0);
	vsm_assert((poll_fd.revents & ~events) == 0);

	return poll_fd.revents;
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
