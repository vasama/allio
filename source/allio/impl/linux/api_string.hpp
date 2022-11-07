#pragma once

#include <allio/detail/dynamic_buffer.hpp>

#include <allio/path_view.hpp>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

class api_string
{
	detail::dynamic_buffer<char, 256> m_buffer;
	detail::linear<size_t> m_size;

public:
	size_t size() const
	{
		return m_size;
	}

	char* data()
	{
		return m_buffer.data();
	}

	char const* data() const
	{
		return m_buffer.data();
	}

	char* c_string()
	{
		m_buffer.data();
	}

	char const* c_string() const
	{
		m_buffer.data();
	}

	operator char*()
	{
		m_buffer.data();
	}

	operator char const*() const
	{
		m_buffer.data();
	}


	result<char*> reserve(size_t size);

	result<char*> set(std::span<std::string_view const> strings);

	result<char*> set(std::string_view const string)
	{
		return set(std::span<std::string_view const>(&string, 1));
	}

	result<char*> set(path_view const path)
	{
		return set(path.string());
	}


	static result<api_string> make(auto const string)
	{
		result<api_string> r(result_value);
		allio_TRYV(r->set(string));
		return r;
	}
};

class api_strings
{
	detail::dynamic_buffer<std::byte> m_buffer;

public:
	char const* c_strings() const
	{
		static constexpr char const* empty = nullptr;
		return m_buffer.size() > 0
			? reinterpret_cast<char const**>(m_buffer.data())
			: &empty;
	}

	operator char const* const*() const
	{
		return c_strings();
	}

	result<char const* const*> set(std::span<std::string_view const> strings);

	static result<api_string> make(std::span<std::string_view const> const strings)
	{
		result<api_string> r(result_value);
		allio_TRYV(r->set(strings));
		return r;
	}
};

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
