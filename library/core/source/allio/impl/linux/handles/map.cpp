#include <allio/detail/handles/map.hpp>

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/new.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/fcntl.hpp>
#include <allio/impl/linux/mman.hpp>

#include <vsm/lazy.hpp>
#include <vsm/numeric.hpp>

#include <linux/mman.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

static vsm::result<int> get_page_protection(protection const protection)
{
	// Linux does not support write only or execute only page protection. mmap can be called with
	// PROT_WRITE or PROT_EXEC and without PROT_READ, but the mapping still provides read access.
	if (protection != protection::none && !vsm::all_flags(protection, protection::read))
	{
		return vsm::unexpected(error::unsupported_operation);
	}

	int page_protection = 0;
	if (vsm::all_flags(protection, protection::read))
	{
		page_protection |= PROT_READ;
	}
	if (vsm::all_flags(protection, protection::write))
	{
		page_protection |= PROT_WRITE;
	}
	if (vsm::all_flags(protection, protection::execute))
	{
		page_protection |= PROT_EXEC;
	}
	return page_protection;
}

using level_flags_pair = std::pair<int, handle_flags>;

static vsm::result<level_flags_pair> get_page_level_flags(page_level const requested_level)
{
	if (requested_level == page_level(0))
	{
		return level_flags_pair{ 0, handle_flags::none };
	}

	auto const supported_levels = get_supported_page_levels();
	if (requested_level == supported_levels.front())
	{
		return level_flags_pair{ 0, handle_flags::none };
	}

	int const mmap_flags =
		MAP_HUGETLB |
		(std::to_underlying(requested_level) << MAP_HUGE_SHIFT & MAP_HUGE_MASK);

	if (supported_levels.size() > 1 && requested_level == supported_levels[1])
	{
		return level_flags_pair{ mmap_flags, map_t::flags::page_level_1 };
	}

	if (supported_levels.size() > 2 && requested_level == supported_levels[2])
	{
		return level_flags_pair{ mmap_flags, map_t::flags::page_level_2 };
	}

	return vsm::unexpected(error::unsupported_operation);
}


static bool check_address_range(native_handle<map_t> const& h, auto const& a)
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
	static_assert(std::numeric_limits<Offset>::max() >= std::numeric_limits<size_t>::max());
	return (offset & (static_cast<Offset>(get_page_size(page_level)) - 1)) == 0;
}

template<std::unsigned_integral Offset>
static Offset align_to_page(Offset const offset, page_level const page_level)
{
	static_assert(std::numeric_limits<Offset>::max() >= std::numeric_limits<size_t>::max());
	return offset & ~(static_cast<Offset>(get_page_size(page_level)) - 1);
}

template<std::unsigned_integral Offset>
static Offset round_to_page(Offset const offset, page_level const page_level)
{
	static_assert(std::numeric_limits<Offset>::max() >= std::numeric_limits<size_t>::max());
	return vsm::po2_ceil(offset, static_cast<Offset>(get_page_size(page_level)));
}


static vsm::result<protection> get_protection(
	protection const section_protection,
	protection const desired_protection)
{
	if (desired_protection == detail::protection(0))
	{
		return section_protection != detail::protection(0)
			? section_protection
			: protection::read_write;
	}

	if (section_protection != detail::protection(0))
	{
		if (!vsm::all_flags(section_protection, desired_protection))
		{
			return vsm::unexpected(error::invalid_argument);
		}
	}

	return desired_protection;
}

static vsm::result<protection> get_protection(
	native_handle<map_t> const& h,
	protection const desired_protection)
{
	return get_protection(
		h.section == nullptr
			? detail::protection(0)
			: h.section->h.protection,
		desired_protection);
}


using map_pair = std::pair<unique_mmap<void>, handle_flags>;

static vsm::result<map_pair> _map_common(
	io_parameters_t<map_t, map_io::map_memory_t> const& a,
	page_level const page_level,
	protection const protection,
	int mmap_flags,
	int const fd,
	off_t const offset)
{
	vsm_try_bind((page_level_flags, h_flags), get_page_level_flags(page_level));

	void* mmap_address = nullptr;
	int mmap_prot = PROT_NONE;

	if (a.address != 0)
	{
		mmap_flags |= MAP_FIXED_NOREPLACE;
		mmap_address = reinterpret_cast<void*>(align_to_page(a.address, page_level));
	}

	if (vsm::any_flags(a.options, map_options::initial_commit))
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
		mmap_flags | page_level_flags,
		fd,
		offset));

	if (mmap_address != nullptr && map.get().base != mmap_address)
	{
		return vsm::unexpected(error::virtual_address_not_available);
	}

	return vsm::result<map_pair>(vsm::result_value, vsm_move(map), h_flags);
}

template<object Object, std::convertible_to<native_handle<Object>> H>
static vsm::result<unique_ptr<shared_native_handle<Object>>> make_shared_handle(H&& h)
{
	using shared_type = shared_native_handle<Object>;
	return make_unique<shared_type>(vsm_lazy(shared_type
	{
		.ref_count = 1,
		.h = vsm_forward(h),
	}));
}

static vsm::result<void> _map_section(
	native_handle<map_t>& h,
	io_parameters_t<map_t, map_io::map_memory_t> const& a)
{
	vsm_assert(a.section != nullptr);

	native_handle<section_t> const& section_h = *a.section;

	if (!section_h.flags[object_t::flags::not_null])
	{
		return vsm::unexpected(error::invalid_argument);
	}

	auto const page_level = a.page_level != detail::page_level(0)
		? a.page_level
		: get_default_page_level();

	if (!is_page_aligned(a.section_offset, page_level))
	{
		return vsm::unexpected(error::invalid_argument);
	}

	auto protection = section_h.protection;

	if (a.protection != detail::protection(0))
	{
		protection = a.protection;
		if (!vsm::all_flags(section_h.protection, protection))
		{
			return vsm::unexpected(error::invalid_argument);
		}
	}

	vsm_try(offset, vsm::try_truncate<off_t>(a.section_offset, error::invalid_argument));

	vsm_try_bind((map, h_flags), _map_common(
		a,
		page_level,
		protection,
		MAP_SHARED_VALIDATE,
		unwrap_handle(section_h.platform_handle),
		offset));

	//TODO: Use a type erased section handle parameter and share already shared handles.
	vsm_try(duplicate_section, linux::duplicate_fd(
		unwrap_handle(section_h.platform_handle),
		/* new_fd: */ -1,
		O_CLOEXEC));

	vsm_try(shared_section, make_shared_handle<section_t>(section_h));
	shared_section->h.platform_handle = wrap_handle(duplicate_section.release());

	h.flags = object_t::flags::not_null | h_flags;
	h.section = shared_section.release();
	h.base = map.get().base;
	h.size = a.size;

	(void)map.release();

	return {};
}

static vsm::result<void> _map_anonymous(
	native_handle<map_t>& h,
	io_parameters_t<map_t, map_io::map_memory_t> const& a)
{
	if (a.section != nullptr)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	if (a.section_offset != 0)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	auto const page_level = a.page_level != detail::page_level(0)
		? a.page_level
		: get_default_page_level();

	auto const protection = a.protection != detail::protection(0)
		? a.protection
		: detail::protection::read_write;

	vsm_try_bind((map, h_flags), _map_common(
		a,
		page_level,
		protection,
		MAP_PRIVATE | MAP_ANONYMOUS,
		/* fd: */ -1,
		/* offset: */ 0));

	// Anonymous mappings are always a multiple of the page size, but _map_common returns a handle
	// containing the size exactly as it was specified. This is good enough for unmapping, but does
	// not adequately describe the usable range of the mapping.
	size_t const map_size = round_to_page(map.get().size, page_level);

	h.flags = object_t::flags::not_null | h_flags;
	h.section = nullptr;
	h.base = map.get().base;
	h.size = map_size;

	(void)map.release();

	return {};
}

vsm::result<void> map_t::map_memory(
	native_handle<map_t>& h,
	io_parameters_t<map_t, map_memory_t> const& a)
{
	if (vsm::any_flags(a.options, map_options::backing_section))
	{
		return _map_section(h, a);
	}
	else
	{
		return _map_anonymous(h, a);
	}
}

vsm::result<void> map_t::commit(
	native_handle<map_t> const& h,
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
	native_handle<map_t> const& h,
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

#if 0
vsm::result<void> map_t::protect(
	native_handle<map_t> const& h,
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
#endif

vsm::result<void> map_t::close(
	native_handle<map_t>& h,
	io_parameters_t<map_t, close_t> const& a)
{
	if (munmap(h.base, h.size) == -1)
	{
		unrecoverable_error(allio_error(get_last_error()));
	}
	h = {};
	return {};
}

page_level map_t::get_page_level(native_handle<map_t> const& h)
{
	auto const supported_levels = get_supported_page_levels();

	if (h.flags[flags::page_level_1])
	{
		return supported_levels[1];
	}

	if (h.flags[flags::page_level_2])
	{
		return supported_levels[2];
	}

	return supported_levels.front();
}
