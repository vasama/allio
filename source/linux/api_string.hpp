#pragma once

#include <string>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

class api_string : std::basic_string<char>
{
public:
	api_string() = default;

	api_string(path_view const path)
	{
		set_string(path);
	}

	using basic_string::data;
	using basic_string::size;

	using basic_string::begin;
	using basic_string::end;

	operator char*()
	{
		return basic_string::data();
	}

	operator char const*() const
	{
		return basic_string::data();
	}

	char* resize(size_t const size)
	{
		basic_string::resize(size);
		return basic_string::data();
	}

	char* set_string(path_view const path)
	{
		return (static_cast<basic_string&>(*this) = path.string()).data();
	}
};

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
