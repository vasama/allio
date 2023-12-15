#pragma once

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/linux/error.hpp>

#include <vsm/lazy.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

#include <limits>
#include <memory>
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

using unique_proc_file = std::unique_ptr<FILE, detail::file_deleter>;

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
	int const r = fscanf(file.get(), format, args...);

	if (r != sizeof...(args))
	{
		return vsm::unexpected(get_last_error());
	}

	return {};
}

} // namespace linux
} // namespace allio

#include <allio/linux/detail/undef.i>
