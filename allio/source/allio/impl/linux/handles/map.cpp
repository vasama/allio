#include <allio/detail/handles/map.hpp>

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/fcntl.hpp>
#include <allio/impl/linux/mman.hpp>

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


template<std::unsigned_integral Offset>
static bool is_page_aligned(Offset const offset, page_level const page_level)
{
	static_assert(std::numeric_limits<Offset>::max() >= std::numeric_limits<size_t>());
	return offset & (static_cast<Offset>(get_page_size(page_level)) - 1);
}

template<std::unsigned_integral Offset>
static Offset align_to_page(Offset const offset, page_level const page_level)
{
	static_assert(std::numeric_limits<Offset>::max() >= std::numeric_limits<size_t>());
	return offset & ~(static_cast<Offset>(get_page_size(page_level)) - 1);
}

template<std::unsigned_integral Offset>
static Offset round_to_page(Offset const offset, page_level const page_level)
{
	static_assert(std::numeric_limits<Offset>::max() >= std::numeric_limits<size_t>());
	return vsm::round_up_to_power_of_two(offset, static_cast<Offset>(get_page_size(page_level)));
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


static vsm::result<unique_mmap<void>> _map_common(
	io_parameters_t<map_t, map_io::map_memory_t> const& a,
	page_level const page_level,
	protection const protection,
	int mmap_flags,
	int const fd,
	off_t const offset)
{
	void* mmap_address = nullptr;
	int mmap_prot = PROT_NONE;

	if (a.address != 0)
	{
		mmap_flags |= MAP_FIXED_NOREPLACE;
		mmap_address = reinterpret_cast<void*>(align_to_page(a.address, page_level));
	}

	if (a.initial_commit)
	{
		vsm_try_assign(mmap_prot, get_page_protection(protection));
	}
	else
	{
		mmap_flags |= MAP_NORESERVE;
	}

	vsm_try(map, linux::mmap(
		mmap_address,
		a.size,
		mmap_prot,
		mmap_flags,
		fd,
		offset));

	if (mmap_address != nullptr && map.get().base != mmap_address)
	{
		return vsm::unexpected(error::virtual_address_not_available);
	}

	return map;
}

static vsm::result<void> _map_section(
	map_t::native_type& h,
	io_parameters_t<map_t, map_io::map_memory_t> const& a)
{
	vsm_assert(a.section != nullptr);

	if (!a.section->flags[object_t::flags::not_null])
	{
		return vsm::unexpected(error::invalid_argument);
	}

	auto const page_level = a.page_level.value_or(vsm_lazy(get_default_page_level()));

	if (!is_page_aligned(a.section_offset, page_level))
	{
		return vsm::unexpected(error::invalid_argument);
	}

	auto const& section_h = *a.section;
	auto protection = section_h.protection;

	if (a.protection)
	{
		protection = a.protection;
		if (!vsm::all_flags(section_h.protection, protection))
		{
			return vsm::unexpected(error::invalid_argument);
		}
	}

	vsm_try(map, _map_common(
		a,
		page_level,
		protection,
		MAP_SHARED_VALIDATE,
		unwrap_handle(section_h.platform_handle),
		a.offset));

	vsm_try(new_fd, linux::duplicate_fd(
		fd,
		/* new_fd: */ -1,
		O_CLOEXEC));

	// Linux maps the entire page but does not persist
	// the bytes mapped past the end of the file.
	size_t const map_size = map.get().size;

	h = native_type
	{
		object_t::native_type
		{
			flags::not_null,
		},
		section_h,
		page_level,
		map.release().base,
		map_size,
	};
	h.section.platform_handle = wrap_handle(new_fd.release());

	return {};
}

static vsm::result<void> _map_anonymous(
	map_t::native_type& h,
	io_parameters_t<map_t, map_io::map_memory_t> const& a)
{
	if (a.section_offset != 0)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	auto const page_level = a.page_level.value_or(vsm_lazy(get_default_page_level()));
	auto const protection = a.protection.value_or(protection::read_write);

	vsm_try(map, _map_common(
		a,
		page_level,
		protection,
		MAP_PRIVATE | MAP_ANONYMOUS,
		/* fd: */ -1,
		/* offset: */ 0));

	// Anonymous mappings are always a multiple of the page size.
	size_t const map_size = round_to_page(map.get().size, page_level);

	h = native_type
	{
		object_t::native_type
		{
			handle_flags(flags::not_null) | map_t::flags::anonymous,
		},
		section_t::native_type{},
		page_level,
		map.release().base,
		map_size,
	};

	return {};
}

vsm::result<void> map_t::map_memory(
	native_type& h,
	io_parameters_t<map_t, map_memory_t> const& a)
{
	if (a.section != nullptr)
	{
		return _map_section(h, a);
	}
	else
	{
		return _map_anonymous(h, a);
	}
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
	h = {};
	return {};
}
