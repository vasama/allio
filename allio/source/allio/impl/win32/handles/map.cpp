#include <allio/detail/handles/map.hpp>

#include <allio/win32/kernel_error.hpp>
#include <allio/win32/platform.hpp>
#include <allio/impl/win32/handles/platform_object.hpp>
#include <allio/impl/win32/kernel.hpp>

#include <vsm/lazy.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

namespace {

enum class mmap_type
{
	section,
	anonymous,
};

struct mmap_view
{
	void* base;
	size_t size;
};

template<mmap_type Type>
struct mmap_deleter
{
	void vsm_static_operator_invoke(mmap_view const view);
};

template<mmap_type Type>
using unique_mmap = vsm::unique_resource<mmap_view, mmap_deleter<Type>>;

using unique_section_mmap = unique_mmap<mmap_type::section>;
using unique_anonymous_mmap = unique_mmap<mmap_type::anonymous>;

} // namespace


static vsm::result<ULONG> get_page_protection(protection const protection)
{
	switch (protection)
	{
	case protection::none:
		return PAGE_NOACCESS;

	case protection::read:
		return PAGE_READONLY;

	case protection::read | protection::write:
		return PAGE_READWRITE;

	case protection::execute:
		return PAGE_EXECUTE;

	case protection::execute | protection::read:
		return PAGE_EXECUTE_READ;

	case protection::execute | protection::read | protection::write:
		return PAGE_EXECUTE_READWRITE;
	}

	return vsm::unexpected(error::unsupported_operation);
}

static vsm::result<ULONG> get_page_level_allocation_type(page_level const level)
{
	auto const supported_levels = get_supported_page_levels();

	if (level == supported_levels.front())
	{
		return 0;
	}

	if (supported_levels.size() > 1)
	{
		// Windows only supports two paging levels.
		vsm_assert(supported_levels.size() == 2);

		if (level == supported_levels.back())
		{
			return MEM_LARGE_PAGES;
		}
	}

	return vsm::unexpected(error::unsupported_page_level);
}


static vsm::result<unique_anonymous_mmap> allocate_virtual_memory(
	HANDLE const process,
	void* base,
	size_t size,
	ULONG const allocation_type,
	ULONG const page_protection)
{
	NTSTATUS const status = NtAllocateVirtualMemory(
		process,
		&base,
		/* ZeroBits: */ 0,
		&size,
		allocation_type,
		page_protection);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	if (allocation_type & MEM_RESERVE)
	{
		return vsm_lazy(unique_anonymous_mmap(mmap_view(base, size)));
	}

	return {};
}

static vsm::result<ULONG> protect_virtual_memory(
	HANDLE const process,
	void* base,
	size_t size,
	ULONG const new_page_protection)
{
	ULONG old_page_protection;
	NTSTATUS const status = NtProtectVirtualMemory(
		process,
		&base,
		&size,
		new_page_protection,
		&old_page_protection);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return old_page_protection;
}

static vsm::result<void> free_virtual_memory(
	HANDLE const process,
	void* base,
	size_t size,
	ULONG const free_type)
{
	NTSTATUS const status = NtFreeVirtualMemory(
		process,
		&base,
		&size,
		free_type);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

static vsm::result<unique_section_mmap> map_view_of_section(
	HANDLE const process,
	HANDLE const section,
	fs_size const offset,
	void* base,
	size_t size,
	ULONG const allocation_type,
	ULONG const page_protection)
{
	LARGE_INTEGER offset_integer;
	offset_integer.QuadPart = offset;

	NTSTATUS const status = NtMapViewOfSection(
		section,
		process,
		&base,
		/* ZeroBits: */ 0,
		size,
		&offset_integer,
		&size,
		ViewUnmap,
		allocation_type,
		page_protection);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return vsm_lazy(unique_section_mmap(mmap_view(base, size)));
}

static vsm::result<void> unmap_view_of_section(
	HANDLE const process,
	void* const base)
{
	NTSTATUS const status = NtUnmapViewOfSection(
		process,
		base);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}


void mmap_deleter<mmap_type::section>::operator()(mmap_view const view) vsm_static_operator_const
{
	unrecoverable(free_virtual_memory(
		GetCurrentProcess(),
		view.base,
		/* size: */ 0,
		MEM_RELEASE));
}

void mmap_deleter<mmap_type::anonymous>::operator()(mmap_view const view) vsm_static_operator_const
{
	unrecoverable(unmap_view_of_section(
		GetCurrentProcess(),
		view.base));
}


static bool check_address_range(map_t::native_type const& h, auto const& a)
{
	uintptr_t const h_beg = reinterpret_cast<uintptr_t>(h.base);
	uintptr_t const h_end = h_beg + h.size;

	uintptr_t const a_beg = reinterpret_cast<uintptr_t>(a.base);
	uintptr_t const a_end = a_beg + a.size;

	return h_beg <= a_beg && a_end <= h_end;
}

static protection get_max_protection(map_t::native_type const& h)
{
	if (h.flags[map_t::flags::anonymous])
	{
		// Page file backed memory can use any protection.
		return protection::read_write | protection::execute;
	}
	else
	{
		return h.section.protection;
	}
}


static vsm::result<void> _map_section(
	map_t::native_type& h,
	io_parameters_t<map_t, map_io::map_memory_t> const& a)
{
	if (!a.initial_commit)
	{
		return vsm::unexpected(error::unsupported_operation);
	}

	if (!a.section->flags[object_t::flags::not_null])
	{
		return vsm::unexpected(error::invalid_argument);
	}

	auto const& section_h = *a.section;

	auto const page_level = a.page_level.value_or(vsm_lazy(get_default_page_level()));
	auto const protection = a.protection.value_or(section_h.protection);

	if (!vsm::all_flags(section_h.protection, protection))
	{
		return vsm::unexpected(error::invalid_argument);
	}

	vsm_try(page_protection, get_page_protection(protection));
	vsm_try(allocation_type, get_page_level_allocation_type(page_level));

	vsm_try(section, win32::duplicate_handle(unwrap_handle(section_h.platform_handle)));

	vsm_try(map, map_view_of_section(
		GetCurrentProcess(),
		section.get(),
		a.section_offset,
		reinterpret_cast<void*>(a.address),
		a.size,
		allocation_type,
		page_protection));

	h = map_t::native_type
	{
		object_t::native_type
		{
			map_t::flags::not_null,
		},
		section_t::native_type
		{
			platform_object_t::native_type
			{
				object_t::native_type
				{
					section_t::flags::not_null,
				},
				wrap_handle(section.release()),
			},
			section_h.protection,
		},
		page_level,
		map.get().base,
		map.get().size,
	};
	(void)map.release();

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

	vsm_try(allocation_type, get_page_level_allocation_type(page_level));
	ULONG page_protection = PAGE_NOACCESS;

	if (a.initial_commit)
	{
		allocation_type |= MEM_COMMIT;
		vsm_try_assign(page_protection, get_page_protection(protection));
	}

	vsm_try(map, allocate_virtual_memory(
		GetCurrentProcess(),
		reinterpret_cast<void*>(a.address),
		a.size,
		allocation_type | MEM_RESERVE,
		page_protection));

	h = map_t::native_type
	{
		object_t::native_type
		{
			handle_flags(map_t::flags::not_null)
				| map_t::flags::anonymous,
		},
		section_t::native_type
		{
		},
		page_level,
		map.get().base,
		map.get().size,
	};
	(void)map.release();

	return {};
}

vsm::result<void> map_t::map_memory(
	native_type& h,
	io_parameters_t<map_t, map_memory_t> const& a)
{
	return a.section != nullptr
		? _map_section(h, a)
		: _map_anonymous(h, a);
}

vsm::result<void> map_t::commit(
	native_type const& h,
	io_parameters_t<map_t, commit_t> const& a)
{
	if (!h.flags[flags::anonymous])
	{
		return vsm::unexpected(error::unsupported_operation);
	}

	if (!check_address_range(h, a))
	{
		return vsm::unexpected(error::invalid_address);
	}

	auto const protection = a.protection.value_or(protection::read_write);
	vsm_try(page_protection, get_page_protection(protection));

	vsm_try_discard(allocate_virtual_memory(
		GetCurrentProcess(),
		a.base,
		a.size,
		MEM_COMMIT,
		page_protection));

	return {};
}

vsm::result<void> map_t::decommit(
	native_type const& h,
	io_parameters_t<map_t, decommit_t> const& a)
{
	if (!h.flags[flags::anonymous])
	{
		return vsm::unexpected(error::unsupported_operation);
	}

	return free_virtual_memory(
		GetCurrentProcess(),
		a.base,
		a.size,
		MEM_DECOMMIT);
}

vsm::result<void> map_t::protect(
	native_type const& h,
	io_parameters_t<map_t, protect_t> const& a)
{
	if (!check_address_range(h, a))
	{
		return vsm::unexpected(error::invalid_address);
	}

	if (!vsm::all_flags(get_max_protection(h), a.protection))
	{
		return vsm::unexpected(error::invalid_argument);
	}

	vsm_try(new_page_protection, get_page_protection(a.protection));

	vsm_try_discard(protect_virtual_memory(
		GetCurrentProcess(),
		a.base,
		a.size,
		new_page_protection));

	return {};
}

vsm::result<void> map_t::close(
	native_type& h,
	io_parameters_t<map_t, close_t> const& a)
{
	if (h.flags[flags::anonymous])
	{
		unrecoverable(free_virtual_memory(
			GetCurrentProcess(),
			h.base,
			h.size,
			MEM_RELEASE));
	}
	else
	{
		unrecoverable(unmap_view_of_section(
			GetCurrentProcess(),
			h.base));

		//TODO: Change close to return void?
		//      It's pretty pointless to return a result if it's always void and never fails.

		(void)section_t::close(h.section, a);
	}
	zero_native_handle(h);
	return {};
}
