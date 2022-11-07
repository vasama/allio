#pragma once

#include <system_error>

#include <cstdint>

namespace allio {
namespace detail {

class nt_error_category : public std::error_category
{
public:
	char const* name() const noexcept override;
	std::string message(int code) const override;

	static nt_error_category const& get()
	{
		return instance;
	}

private:
	static nt_error_category const instance;
};

} // namespace detail
namespace win32 {

enum class nt_error : long
{
	none = 0,
};

inline std::error_code make_error_code(nt_error const error)
{
	return std::error_code(static_cast<int>(error), detail::nt_error_category::get());
}

inline std::error_category const& nt_category()
{
	return detail::nt_error_category::get();
}

} // namespace win32
} // namespace allio

template<>
struct std::is_error_code_enum<allio::win32::nt_error>
{
	static constexpr bool value = true;
};
