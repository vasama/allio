#pragma once

#include <allio/path_view.hpp>

#include <string>

namespace allio::win32 {

class api_string : std::basic_string<wchar_t>
{
public:
	api_string() = default;

	api_string(char const* const c_str)
	{
		set(std::string_view(c_str));
	}

	api_string(wchar_t const* const c_str)
	{
		set(std::wstring_view(c_str));
	}

	api_string(std::string_view const string)
	{
		set(string);
	}

	api_string(std::wstring_view const string)
	{
		set(string);
	}

	api_string(path_view const path)
	{
		set(path);
	}

	api_string(platform_path_view const path)
	{
		set(path);
	}


	using basic_string::data;
	using basic_string::size;

	using basic_string::begin;
	using basic_string::end;

	operator wchar_t*()
	{
		return basic_string::data();
	}

	operator wchar_t const*() const
	{
		return basic_string::data();
	}


	wchar_t* resize(size_t const size)
	{
		basic_string::resize(size);
		return basic_string::data();
	}


	void append(char const* const c_str)
	{
		append(std::string_view(c_str));
	}

	void append(wchar_t const* const c_str)
	{
		append(std::wstring_view(c_str));
	}

	void append(std::string_view const string);

	void append(std::wstring_view const string)
	{
		basic_string::append(string);
	}

	void append(path_view const path)
	{
		append(path.string());
	}

	void append(platform_path_view const path)
	{
		append(path.string());
	}


	void set(char const* const c_str)
	{
		set(std::string_view(c_str));
	}

	void set(wchar_t const* const c_str)
	{
		set(std::wstring_view(c_str));
	}

	wchar_t* set(std::string_view const string)
	{
		basic_string::clear();
		append(string);
		return basic_string::data();
	}

	wchar_t* set(std::wstring_view const string)
	{
		basic_string::clear();
		append(string);
		return basic_string::data();
	}

	wchar_t* set(path_view const path)
	{
		return set(path.string());
	}

	wchar_t* set(platform_path_view const path)
	{
		return set(path.string());
	}
};

} // namespace allio::win32
