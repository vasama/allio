#pragma once

#include <allio/impl/win32/kernel.hpp>

#include <vsm/lazy.hpp>
#include <vsm/result.hpp>

namespace allio::win32 {

static constexpr ULONG timer2_flag_2                        = 1 << 2;
static constexpr ULONG timer2_flag_3                        = 1 << 3;
static constexpr ULONG timer2_manual_reset                  = 1 << 31;

inline vsm::result<unique_timer> create_timer()
{
	HANDLE handle;
	NTSTATUS const status = NtCreateTimer2(
		&handle,
		/* Reserved: */ nullptr,
		/* ObjectAttributes: */ nullptr,
		timer2_flag_2,
		TIMER_MODIFY_STATE | SYNCHRONIZE);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return vsm_lazy(unique_timer(wrap_timer(handle)));
}

inline vsm::result<void> set_timer(timer const timer, deadline const deadline)
{
	T2_SET_PARAMETERS parameters =
	{
		.Version = T2_SET_PARAMETERS_VERSION,
		.NoWakeTolerance = 0, //TODO: Set NoWakeTolerance
	};

	NTSTATUS const status = NtSetTimer2(
		unwrap_timer(timer),
		kernel_timeout(deadline),
		/* Period: */ nullptr,
		&parameters);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

} // namespace allio::win32
