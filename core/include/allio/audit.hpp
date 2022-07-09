#pragma once

#include <source_location>

namespace allio {

struct audit_information
{
	char const* operation;
	std::source_location location;
};

using audit_callback = void(audit_information const& info);

} // namespace allio
