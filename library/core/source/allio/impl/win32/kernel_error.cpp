#include <allio/win32/kernel_error.hpp>

#include <allio/impl/win32/kernel.hpp>

#include <format>
#include <memory>

using namespace allio;
using namespace allio::win32;

static std::string get_win32_error_message(ULONG const error) noexcept
{
#if _MSVC_STL_UPDATE >= 202501L
	return std::system_category().message(static_cast<int>(error));
#else
	static constexpr DWORD flags =
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS;

	struct message_deleter
	{
		void operator()(void* const storage) const
		{
			LocalFree(storage);
		}
	};
	std::unique_ptr<char, message_deleter> p_message;

	DWORD const message_size = FormatMessageA(
		flags,
		/* lpSource: */ nullptr,
		error,
		/* dwLanguageId: */ 0x0409, // en-US
		reinterpret_cast<char*>(static_cast<char**>(std::out_ptr(p_message))),
		/* nSize: */ 0,
		/* Arguments: */ nullptr);

	if (message_size != 0)
	{
		return std::string(p_message.get(), message_size);
	}

	return {};
#endif
}

char const* detail::kernel_error_category::name() const noexcept
{
	return "Windows NT Kernel";
}

std::string detail::kernel_error_category::message(int const code) const
{
	ULONG const win32_error = win32::RtlNtStatusToDosError(static_cast<NTSTATUS>(code));
	if (win32_error != ERROR_MR_MID_NOT_FOUND)
	{
		if (std::string message = get_win32_error_message(win32_error); !message.empty())
		{
			return message;
		}
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
