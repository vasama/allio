#pragma once

#include <system_error>

#include <cstdint>

namespace allio::win32 {

enum class kernel_error : int32_t
{
};

class kernel_error_category : public std::error_category
{
public:
	char const* name() const noexcept override;
	std::string message(int code) const override;

	static kernel_error_category const& get()
	{
		return instance;
	}

private:
	static kernel_error_category const instance;
};

inline std::error_code make_error_code(kernel_error const error)
{
	return std::error_code(static_cast<int>(error), kernel_error_category::get());
}

} // namespace allio::win32

template<>
struct std::is_error_code_enum<allio::win32::kernel_error>
{
	static constexpr bool value = true;
};
