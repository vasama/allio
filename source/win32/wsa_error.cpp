#include "wsa_error.hpp"

using namespace allio;
using namespace allio::win32;

char const* wsa_error_category::name() const noexcept
{
	return "Windows WSA";
}

std::string wsa_error_category::message(int const code) const
{
	return std::system_category().message(code);
}

wsa_error_category const wsa_error_category::instance;
