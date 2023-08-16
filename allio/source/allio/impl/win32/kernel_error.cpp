#include <allio/win32/kernel_error.hpp>

#include <allio/impl/win32/kernel.hpp>

#include <format>

using namespace allio;
using namespace allio::win32;

char const* detail::nt_error_category::name() const noexcept
{
	return "Windows NT Kernel";
}

std::string detail::nt_error_category::message(int const code) const
{
	ULONG const win32_error = win32::RtlNtStatusToDosError(static_cast<NTSTATUS>(code));
	if (win32_error != ERROR_MR_MID_NOT_FOUND)
	{
		return std::system_category().message(static_cast<int>(win32_error));
	}
	return std::format("NTSTATUS:{:08X}", static_cast<uint32_t>(code));
}

detail::nt_error_category const detail::nt_error_category::instance;
