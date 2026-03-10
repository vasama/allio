#include <allio/detail/filesystem.hpp>

#include <vsm/lift.hpp>

#include <ranges>

using namespace allio;
using namespace allio::detail;

template<typename Char>
static bool _is_null_device_path(std::basic_string_view<Char> const path)
{
	auto const to_upper_or_zero = [](char32_t character)
	{
		if ('a' <= character && character <= 'z')
		{
			character = 'A' + (character - 'a');
		}
	
		return character >= 0x100
			? 0
			: static_cast<uint8_t>(character);
	};

	auto const test = [&](std::string_view const null_path)
	{
		return std::ranges::equal(
			null_path,
			std::views::transform(path, to_upper_or_zero));
	};

	return test("NUL") || test("\\??\\DEVICE\\NULL");
}

static bool _is_null_device_path(string_length_out_of_range_t)
{
	return false;
}

bool detail::is_null_device_path(any_path_view const path)
{
	return path.string().visit(vsm_lift(_is_null_device_path));
}


vsm::result<size_t> detail::canonical_path(fs_path const& path, any_path_buffer const buffer)
{
	
}

vsm::result<size_t> detail::weakly_canonical_path(fs_path const& path, any_path_buffer const buffer)
{
	
}

vsm::result<size_t> detail::relative_path(
	fs_path const& path,
	fs_path const& base,
	any_path_buffer const buffer)
{
}

vsm::result<size_t> detail::proximate_path(
	fs_path const& path,
	fs_path const& base,
	any_path_buffer const buffer)
{
}
