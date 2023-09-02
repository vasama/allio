#pragma once

#include <allio/linux/detail/unique_fd.hpp>

#include <vsm/lazy.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

#include <string_view>

#include <allio/linux/detail/undef.i>

namespace allio {
namespace detail {

struct file_deleter
{
	void vsm_static_operator_invoke(FILE* const file)
	{
		fclose(file);
	}
};

} // namespace detail

namespace linux {

using unique_proc_file = unique_ptr<FILE, detail::file_deleter>;

template<size_t Size>
vsm::result<unique_proc_file> proc_open(
	char const(&format)[Size],
	std::convertible_to<int> auto const... args)
{
	// Max number of digits in an int, including +1 for the sign.
	static constexpr size_t max_int_digits = std::numeric_limits<int>::digits + 1;

	char buffer[Size + sizeof...(args) * max_int_digits];
	vsm_verify(snprintf(buffer, format, static_cast<int>(args)...) > 0);

	FILE* const file = fopen(buffer, "r");

	if (file == nullptr)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm_lazy(unique_proc_file(file));
}

vsm::result<void> proc_scan(
	unique_proc_file const& file,
	char const* const format,
	auto const&... args)
{
	int const r = fscanf(file.get(), format.format, args...);

	if (r != sizeof...(args))
	{
		vsm::unexpected(get_last_error());
	}

	return {};
}

} // namespace linux
} // namespace allio

#if 0
namespace allio::linux {

class proc_scanner
{
	detail::unique_fd m_fd;

	std::unique_ptr<char> m_dynamic;
	union
	{
		char m_static[1024];
		size_t m_dynamic_size;
	};

	size_t m_data_beg = 0;
	size_t m_data_end = 0;

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

	vsm::result<std::string_view> _scan(std::string_view field);
	vsm::result<bool> scan_string(std::string_view string);
	
	std::span<char> get_buffer();
	size_t advance(size_t size);

	vsm::result<std::string_view> read_some();
	vsm::result<std::string_view> read_more();
	
	vsm::result<size_t> read(std::span<char> buffer);
};

} // namespace allio::linux
#endif

#include <allio/linux/detail/undef.i>
