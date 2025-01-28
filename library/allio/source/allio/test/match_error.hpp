#pragma once

#include <format>
#include <system_error>

namespace allio {

class match_error
{
	std::error_condition m_error_condition;

public:
	template<typename ErrorCode>
	explicit match_error(ErrorCode const& error_code)
		requires requires { make_error_condition(error_code); }
		: m_error_condition(make_error_condition(error_code))
	{
	}

	explicit match_error(std::error_condition const error_condition)
		: m_error_condition(error_condition)
	{
	}

	bool match(std::system_error const& e) const
	{
		return e.code().default_error_condition() == m_error_condition;
	}

	std::string toString() const
	{
		return std::format("Equals: '{}'", m_error_condition.message());
	}
};

} // namespace allio
