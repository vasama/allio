#pragma once

#include <allio/deadline.hpp>

namespace allio {

template<typename Timespec>
[[nodiscard]] Timespec make_timespec(deadline::duration const duration)
{
	using tv_sec_type = std::chrono::duration<
		decltype(Timespec::tv_sec),
		std::chrono::seconds::period>;

	using tv_nsec_type = std::chrono::duration<
		decltype(Timespec::tv_nsec),
		std::chrono::nanoseconds::period>;

	auto const nano_duration = duration % std::chrono::seconds(1);

	return Timespec
	{
		.tv_sec = std::chrono::duration_cast<tv_sec_type>(duration).count(),
		.tv_nsec = std::chrono::duration_cast<tv_nsec_type>(nano_duration).count(),
	};
}

template<typename Timespec>
[[nodiscard]] Timespec make_timespec(deadline const deadline)
{
	vsm_assert(deadline.is_relative());
	return make_timespec<Timespec>(deadline.relative());
}

} // namespace allio
