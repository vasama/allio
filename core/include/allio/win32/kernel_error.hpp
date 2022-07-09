#pragma once

#include <vsm/result.hpp>

#include <cstdint>

namespace allio {
namespace detail {

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

} // namespace detail
namespace win32 {

enum class kernel_error : long
{
	none = 0,
};

inline std::error_code make_error_code(kernel_error const error)
{
	return std::error_code(static_cast<int>(error), detail::kernel_error_category::get());
}

inline std::error_category const& nt_category()
{
	return detail::kernel_error_category::get();
}

template<typename T>
using kernel_result = vsm::result<T, kernel_error>;

} // namespace win32
} // namespace allio

template<>
struct std::is_error_code_enum<allio::win32::kernel_error>
{
	static constexpr bool value = true;
};
