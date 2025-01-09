#include <allio/win32/kernel_error.hpp>

#include <allio/impl/win32/kernel.hpp>

#include <format>

using namespace allio;
using namespace allio::win32;

char const* detail::kernel_error_category::name() const noexcept
{
	return "Windows NT Kernel";
}

std::string detail::kernel_error_category::message(int const code) const
{
	ULONG const win32_error = win32::RtlNtStatusToDosError(static_cast<NTSTATUS>(code));
	if (win32_error != ERROR_MR_MID_NOT_FOUND)
	{
		return std::system_category().message(static_cast<int>(win32_error));
	}

	return std::format("NTSTATUS:{:08X}", static_cast<uint32_t>(code));
}

std::error_condition detail::kernel_error_category::default_error_condition(
	int const code) const noexcept
{
	switch (static_cast<NTSTATUS>(code))
	{
	case STATUS_NO_MEMORY:
		return std::errc::not_enough_memory;

	case STATUS_OBJECT_NAME_NOT_FOUND:
	case STATUS_OBJECT_PATH_NOT_FOUND:
		return std::errc::no_such_file_or_directory;
	}

	ULONG const win32_error = win32::RtlNtStatusToDosError(static_cast<NTSTATUS>(code));
	if (win32_error != ERROR_MR_MID_NOT_FOUND)
	{
		auto const win32_code = static_cast<int>(win32_error);
		auto const win32_condition = std::system_category().default_error_condition(win32_code);

		if (win32_condition.category() == std::generic_category())
		{
			return win32_condition;
		}
	}

	return std::error_condition(code, *this);
}

detail::kernel_error_category const detail::kernel_error_category::instance;
