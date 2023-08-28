#pragma once

#include <allio/linux/detail/unique_fd.hpp>

#include <vsm/result.hpp>

#include <string_view>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

class proc_scanner
{
	static constexpr size_t buffer_size = 1024;

	detail::unique_fd m_fd;

	char m_buffer[1024];
	size_t m_buffer_offset = 0;

public:
	static vsm::result<proc_scanner> open(char const* path);

	proc_scanner(proc_scanner const&) = delete;
	proc_scanner& operator=(proc_scanner const&) = delete;

	template<typename T>
	vsm::result<T> scan(std::string_view const field)
	{
		return scan(field, []()
		{

		});
	}

	template<typename Parser>
	vsm::result<std::invoke_result_t<Parser&&>> scan(std::string_view const field, Parser&& parser)
	{
		vsm_try(value, _scan(field));
		return vsm_forward(parser)(value);
	}

private:
	explicit proc_scanner(int const fd)
		: m_fd(fd)
	{
	}

	vsm::result<std::string_view> _scan_field(std::string_view field);
};

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
