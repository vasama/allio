#include <allio/win32/kernel_error.hpp>

#include <format>

#include "kernel.hpp"

using namespace allio;
using namespace allio::win32;

char const* kernel_error_category::name() const noexcept
{
	return "Windows NT Kernel";
}

std::string kernel_error_category::message(int const code) const
{
	ULONG const win32 = RtlNtStatusToDosError(static_cast<NTSTATUS>(code));

	if (win32 != ERROR_MR_MID_NOT_FOUND)
	{
		return std::system_category().message(static_cast<int>(win32));
	}

	return std::format("NTSTATUS:{:08x}", static_cast<uint32_t>(code));
}

kernel_error_category const kernel_error_category::instance;
