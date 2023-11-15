#include <allio/memory.hpp>

//#include <allio/directory.hpp>
#include <allio/impl/bounded_vector.hpp>

#include <unistd.h>

using namespace allio;

template<size_t MaxSize>
using supported_page_levels = bounded_vector<page_level, MaxSize>;

static vsm::result<supported_page_levels<2>> get_supported_huge_page_levels()
{
	supported_page_levels<2> levels;

#if 0
	vsm_try(directory, open_directory("/sys/kernel/mm/hugepages"));

	directory_stream_buffer stream_buffer;

	while (true)
	{
		vsm_try(stream, directory.read(stream_buffer));

		if (!stream)
		{
			break;
		}

		for (directory_entry const entry : stream)
		{
			std::string_view name = entry.name.utf8_view();

			static constexpr std::string_view prefix = "hugepages-";
			if (!name.starts_with(prefix))
			{
				return vsm::unexpected(allio::unknown_error);
			}
			name.remove_prefix(prefix);

			static constexpr std::string_view suffix = "kB";
			if (!name.starts_with(suffix))
			{
				return vsm::unexpected(allio::unknown_error);
			}
			name.remove_prefix(suffix);

			vsm_try(page_size, vsm::from_chars<size_t>(name));

			if (!vsm::math::is_power_of_two(page_size))
			{
				return vsm::unexpected(allio::unknown_error);
			}

			if (page_size > static_cast<size_t>(-1) / 1024)
			{
				return vsm::unexpected(allio::unknown_error);
			}

			if (!levels.push_back(get_page_level(page_size * 1024)))
			{
				return vsm::unexpected(allio::unknown_error);
			}
		}
	}

	std::ranges::sort(levels);
#endif

	return levels;
}


page_level allio::get_default_page_level()
{
	static page_level const value = get_page_level(sysconf(_SC_PAGE_SIZE));
	return value;
}

std::span<page_level const> allio::get_supported_page_levels()
{
	static auto const value = []() -> supported_page_levels<3>
	{
		supported_page_levels<3> levels;

		levels.push_back(get_default_page_level());

		if (auto r = get_supported_huge_page_levels())
		{
			std::span const huge_levels = *r;
			vsm_assert(huge_levels.front() != levels.front());
			levels.append_range(huge_levels);
		}

		return levels;
	}();
	return value;
}

size_t allio::get_allocation_granularity(page_level const level)
{
	return get_page_size(level);
}
