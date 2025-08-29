#include <allio/detail/memory.hpp>

//#include <allio/directory.hpp>
#include <allio/impl/inplace_vector.hpp>

#include <unistd.h>

using namespace allio;
using namespace allio::detail;

template<size_t MaxSize>
using supported_page_levels = inplace_vector<page_level, MaxSize>;

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


page_level detail::get_default_page_level()
{
	static page_level const value = get_page_level(static_cast<size_t>(sysconf(_SC_PAGE_SIZE)));
	return value;
}

std::span<page_level const> detail::get_supported_page_levels()
{
	static auto const value = []() -> supported_page_levels<3>
	{
		supported_page_levels<3> levels;
		levels.unchecked_push_back(get_default_page_level());

		if (auto const huge = get_supported_huge_page_levels(); huge && !huge->empty())
		{
			vsm_assert(huge->front() > levels.front());

			for (page_level const huge_level : *huge)
			{
				levels.unchecked_push_back(huge_level);
			}
		}

		return levels;
	}();
	return value;
}

size_t detail::get_allocation_granularity(page_level const level)
{
	return get_page_size(level);
}
