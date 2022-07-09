#pragma once

#include <system_error>

namespace allio::win32 {

class wsa_error_category : public std::error_category
{
public:
	char const* name() const noexcept override;
	std::string message(int code) const override;

	static wsa_error_category const& get()
	{
		return instance;
	}

private:
	static wsa_error_category const instance;
};

} // namespace allio::win32
