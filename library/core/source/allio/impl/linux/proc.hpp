#pragma once

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/linux/error.hpp>

#include <vsm/lazy.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

#include <limits>
#include <memory>
#include <string_view>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

struct proc_file_deleter
{
	vsm_static_operator void operator()(FILE* const file) vsm_static_operator_const
	{
		fclose(file);
	}
};
using unique_proc_file = std::unique_ptr<FILE, proc_file_deleter>;

template<size_t FormatSize>
[[nodiscard]] auto make_proc_path(
	char const(&format)[FormatSize],
	std::same_as<int> auto const... args)
{
	// Max number of digits in an int, including +1 for the sign.
	constexpr size_t max_int_digits = std::numeric_limits<int>::digits + 1;
	constexpr size_t max_path_size = FormatSize + sizeof...(args) * max_int_digits;

	vsm_gnu_diagnostic(push)
	vsm_gnu_diagnostic(ignored "-Wformat-nonliteral")

	std::array<char, max_path_size> path;
	vsm_verify(std::snprintf(path.data(), path.size(), format, args...) > 0);

	vsm_gnu_diagnostic(pop)

	return path;
}

template<size_t Size>
[[nodiscard]] vsm::result<unique_proc_file> proc_open(
	char const(&format)[Size],
	std::same_as<int> auto const... args)
{
	FILE* const file = fopen(make_proc_path(format, args...).data(), "r");

	if (file == nullptr)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	return vsm_lazy(unique_proc_file(file));
}

[[nodiscard]] vsm::result<void> proc_scan(
	unique_proc_file const& file,
	char const* const format,
	auto const&... args)
{
	int const r = fscanf(file.get(), format, args...);

	if (r != sizeof...(args))
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	return {};
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
