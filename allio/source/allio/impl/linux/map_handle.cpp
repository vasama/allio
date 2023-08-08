#include <allio/map_handle.hpp>

#include <allio/directory_handle.hpp>
#include <allio/impl/bounded_vector.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/linux/detail/unique_fd.hpp>
#include <allio/linux/platform.hpp>

#include <vsm/defer.hpp>

#include <sys/mman.h>
#include <unistd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

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

std::span<page_level const> allio::get_supported_page_levels()
{
	static auto const levels = []() -> supported_page_levels<3>
	{
		supported_page_levels<3> levels;

		size_t const small_page_size = sysconf(_SC_PAGE_SIZE);
		levels.push_back(get_page_level(small_page_size));

		if (auto r = get_supported_huge_page_levels())
		{
			std::span const huge_levels = *r;
			vsm_assert(huge_levels.front() != levels.front());
			levels.append_range(huge_levels);
		}

		return levels;
	}();

	return levels;
}


static int get_page_protection(page_access const access)
{
	int p = 0;
	if (vsm::any_flags(access, page_access::read))
	{
		p |= PROT_READ;
	}
	if (vsm::any_flags(access, page_access::write))
	{
		p |= PROT_WRITE;
	}
	if (vsm::any_flags(access, page_access::execute))
	{
		p |= PROT_EXEC;
	}
	return p;
}

static int get_page_level_flags(page_level const level)
{
	if (level == page_level::default_level)
	{
		return 0;
	}

	auto const supported_levels = get_supported_page_levels();

	if (level == supported_levels.front())
	{
		return 0;
	}

	return MAP_HUGETLB | std::to_underlying(level) << MAP_HUGE_SHIFT;
}


vsm::result<void> map_handle_base::set_page_access(void* const base, size_t const size, page_access const access)
{
	if (!*this)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	if (!check_address_range(base, size))
	{
		return vsm::unexpected(error::invalid_address);
	}

	if (mprotect(base, size, get_page_protection(access)) == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return {};
}


vsm::result<void> detail::map_handle_base::sync_impl(io::parameters_with_result<io::map> const& args)
{
	map_handle& h = *args.handle;

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}


	int flags = MAP_SHARED_VALIDATE;
	int fd = -1;


	if (args.section != nullptr)
	{
		section_handle const& sh = *args.section;

		if (!sh)
		{
			return vsm::unexpected(error::invalid_argument);
		}

		fd = unwrap_handle(sh.get_platform_handle());
	}
	else
	{
		if (args.offset != 0)
		{
			return vsm::unexpected(error::invalid_argument);
		}

		flags |= MAP_ANONYMOUS;
	}

	void* const requested_address = reinterpret_cast<void*>(args.address);

	if (requested_address != nullptr)
	{
		flags |= MAP_FIXED_NOREPLACE;
	}

	flags |= get_page_level_flags(args.page_level);


	void* base = mmap(
		requested_address,
		args.size,
		get_page_protection(args.access),
		flags,
		fd,
		args.offset);

	if (base == MAP_FAILED)
	{
		return vsm::unexpected(get_last_error());
	}

	vsm_defer
	{
		if (base != nullptr)
		{
			vsm_verify(munmap(base, args.size) != -1);
		}
	};

	if (requested_address != nullptr && base != requested_address)
	{
		return vsm::unexpected(error::virtual_address_not_available);
	}


	h.m_base.value = std::exchange(base, nullptr);
	h.m_size.value = args.size;

	return {};
}

vsm::result<void> detail::map_handle_base::sync_impl(io::parameters_with_result<io::close> const& args)
{
	map_handle& h = static_cast<map_handle&>(*args.handle);

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	if (munmap(h.m_base.value, h.m_size.value) == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	h.m_base.value = nullptr;
	h.m_size.value = 0;

	return {};
}
