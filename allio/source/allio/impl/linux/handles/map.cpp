#include <allio/detail/handles/map.hpp>

#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/fcntl.hpp>
#include <allio/impl/linux/mman.hpp>
#include <allio/linux/detail/unique_fd.hpp>
#include <allio/linux/platform.hpp>

#include <vsm/lazy.hpp>
#include <vsm/math.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

static vsm::result<int> get_page_protection(protection const protection)
{
	// Linux does not support write only or execute only page protection.
	if (protection != protection::none &&
		vsm::no_flags(protection, protection::read))
	{
		return vsm::unexpected(error::unsupported_operation);
	}

	int page_protection = 0;
	if (vsm::any_flags(protection, protection::read))
	{
		page_protection |= PROT_READ;
	}
	if (vsm::any_flags(protection, protection::write))
	{
		page_protection |= PROT_WRITE;
	}
	if (vsm::any_flags(protection, protection::execute))
	{
		page_protection |= PROT_EXEC;
	}
	return page_protection;
}

static int get_page_level_flags(std::optional<page_level> const level)
{
	if (!level)
	{
		return 0;
	}

	auto const supported_levels = get_supported_page_levels();

	if (*level == supported_levels.front())
	{
		return 0;
	}

	return MAP_HUGETLB | std::to_underlying(*level) << MAP_HUGE_SHIFT;
}


static bool check_address_range(map_t::native_type const& h, auto const& a)
{
	uintptr_t const h_beg = reinterpret_cast<uintptr_t>(h.base);
	uintptr_t const h_end = h_beg + h.size;

	uintptr_t const a_beg = reinterpret_cast<uintptr_t>(a.base);
	uintptr_t const a_end = a_beg + a.size;

	return h_beg <= a_beg && a_end <= h_end;
}


vsm::result<void> map_t::map_memory(
	native_type& h,
	io_parameters_t<map_t, map_memory_t> const& a)
{
	section_t::native_type section_h;
	handle_flags h_flags = {};

	protection default_protection;
	protection maximum_protection;

	int fd = -1;
	int mmap_flags = 0;

	if (a.section != nullptr)
	{
		if (!a.section->flags[object_t::flags::not_null])
		{
			return vsm::unexpected(error::invalid_argument);
		}

		section_h = *a.section;

		default_protection = section_h.protection;
		maximum_protection = section_h.protection;

		fd = unwrap_handle(section_h.platform_handle);

		mmap_flags |= MAP_SHARED_VALIDATE;
	}
	else
	{
		if (a.section_offset != 0)
		{
			return vsm::unexpected(error::invalid_argument);
		}

		section_h = {};
		h_flags |= flags::anonymous;

		default_protection = protection::read_write;
		maximum_protection = protection::read_write | protection::execute;

		mmap_flags |= MAP_PRIVATE | MAP_ANONYMOUS;
	}

	void* address = nullptr;
	if (a.address != 0)
	{
		mmap_flags |= MAP_FIXED_NOREPLACE;
		address = reinterpret_cast<void*>(a.address);
	}

	auto const protection = a.protection.value_or(default_protection);
	if (!vsm::all_flags(maximum_protection, protection))
	{
		vsm::unexpected(error::invalid_argument);
	}

	auto const page_level = a.page_level.value_or(vsm_lazy(get_default_page_level()));
	mmap_flags |= get_page_level_flags(page_level);

	int mmap_protection = PROT_NONE;
	if (a.initial_commit)
	{
		vsm_try_assign(mmap_protection, get_page_protection(protection));
	}
	else
	{
		mmap_flags |= MAP_NORESERVE;
	}

	vsm_try(map, linux::mmap(
		address,
		a.size,
		mmap_protection,
		mmap_flags,
		fd,
		a.section_offset));

	if (address != nullptr && map.get().base != address)
	{
		return vsm::unexpected(error::virtual_address_not_available);
	}

	if (fd != -1)
	{
		vsm_try(new_fd, linux::duplicate_fd(
			fd,
			/* new_fd: */ -1,
			O_CLOEXEC));

		section_h.platform_handle = wrap_handle(new_fd.release());
	}

	h = native_type
	{
		object_t::native_type
		{
			flags::not_null,
		},
		section_h,
		page_level,
		map.get().base,
		vsm::round_up_to_power_of_two(map.get().size, get_page_size(page_level)),
	};
	(void)map.release();

	return {};
}

static vsm::result<protection> get_protection(
	std::optional<protection> const section_protection,
	std::optional<protection> const desired_protection)
{
	if (!desired_protection)
	{
		return section_protection.value_or(protection::read_write);
	}

	if (section_protection)
	{
		if (!vsm::all_flags(*section_protection, *desired_protection))
		{
			return vsm::unexpected(error::invalid_argument);
		}
	}

	return *desired_protection;
}

static vsm::result<protection> get_protection(
	map_t::native_type const& h,
	std::optional<protection> const desired_protection)
{
	return get_protection(
		h.flags[map_t::flags::anonymous]
			? std::optional<protection>(h.section.protection)
			: std::optional<protection>(),
		desired_protection);
}

vsm::result<void> map_t::commit(
	native_type const& h,
	io_parameters_t<map_t, commit_t> const& a)
{
	if (!check_address_range(h, a))
	{
		return vsm::unexpected(error::invalid_address);
	}

	vsm_try(protection, get_protection(h, a.protection));
	vsm_try(page_protection, get_page_protection(protection));

	return linux::mprotect(
		a.base,
		a.size,
		page_protection);
}

vsm::result<void> map_t::decommit(
	native_type const& h,
	io_parameters_t<map_t, decommit_t> const& a)
{
	if (!check_address_range(h, a))
	{
		return vsm::unexpected(error::invalid_address);
	}

	vsm_try_void(linux::mprotect(
		a.base,
		a.size,
		PROT_NONE));

	unrecoverable(linux::madvise(
		a.base,
		a.size,
		MADV_DONTNEED));

	return {};
}

vsm::result<void> map_t::protect(
	native_type const& h,
	io_parameters_t<map_t, protect_t> const& a)
{
	if (!check_address_range(h, a))
	{
		return vsm::unexpected(error::invalid_address);
	}

	vsm_try(page_protection, get_page_protection(a.protection));

	vsm_try_void(linux::mprotect(
		a.base,
		a.size,
		page_protection));

	return {};
}

vsm::result<void> map_t::close(
	native_type& h,
	io_parameters_t<map_t, close_t> const& a)
{
	if (munmap(h.base, h.size) == -1)
	{
		unrecoverable_error(get_last_error());
	}
	zero_native_handle(h);
	return {};
}
