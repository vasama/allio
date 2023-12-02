#include <allio/detail/handles/map.hpp>

#include <allio/win32/kernel_error.hpp>
#include <allio/win32/platform.hpp>
#include <allio/impl/win32/kernel.hpp>

#include <vsm/defer.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

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


static vsm::result<unique_mmap<void>> allocate_virtual_memory(
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

	return unique_mmap<void>{ base, size };
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
		old_page_protection);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return old_page_protection;
}

static vsm::result<void> free_virtual_memory(
	HANDLE const process,
	void* base,
	size_t size)
{
	NTSTATUS const status = NtFreeVirtualMemory(
		process,
		&base,
		&size,
		MEM_RELEASE);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

static vsm::result<unique_mmap<void>> map_view_of_section(
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

	return unique_mmap<void>{ base, size };
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
		return h.section.protection;
	}
	else
	{
		// Page file backed memory can use any protection.
		return protection::read_write | protection::execute;
	}
}


static vsm::result<void> _map_section(
	map_t::native_type& h,
	io_parameters_t<map_t, map_io::map_memory_t> const& a)
{
	auto const& section_h = *h.section;

	if (!section_h.flags[section_t::flags::not_null])
	{
		return vsm::unexpected(error::invalid_argument);
	}

	if (!a.commit)
	{
		return vsm::unexpected(error::unsupported_operation);
	}

	auto const page_level = a.page_level.value_or(vsm_lazy(get_default_page_level()));
	vsm_try(page_protection, get_page_protection(a.protection));
	vsm_try(allocation_type, get_page_level_allocation_type(page_level));

	vsm_try(section, duplicate_handle(unwrap_handle(section.h_platform_handle)));

	vsm_try(map, map_view_of_section(
		GetCurrentProcess(),
		section.get(),
		a.offset,
		reinterpret_cast<void*>(a.address),
		a.size,
		allocation_type | MEM_RESERVE,
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
	if (a.offset != 0)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	auto const page_level = a.page_level.value_or(vsm_lazy(get_default_page_level()));
	vsm_try(page_protection, get_page_protection(a.protection));
	vsm_try(allocation_type, get_page_level_allocation_type(page_level));

	if (a.commit)
	{
		allocation_type |= MEM_COMMIT;
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
	io_parameters_t<map_t, commit_t> const& a) const
{
	if (!h.flags[flags::anonymous])
	{
		return vsm::unexpected(error::unsupported_operation);
	}

	if (!check_address_range(h, a))
	{
		return vsm::unexpected(error::invalid_address);
	}

	vsm_try(page_protection, get_page_protection(protection));

	//TODO: non-owning unique_mmap
	vsm_try(map, allocate_virtual_memory(
		GetCurrentProcess(),
		,
		/* ZeroBits: */ 0,
		,
		MEM_COMMIT,
		page_protection));

	return {};
}

vsm::result<void> map_t::decommit(
	native_type const& h,
	io_parameters_t<map_t, decommit_t> const& a) const
{
	return vsm::unexpected(error::unsupported_operation);
}

vsm::result<void> map_t::protect(
	native_type const& h,
	io_parameters_t<map_t, protect_t> const& a) const
{
	if (!check_address_range(h, a))
	{
		return vsm::unexpected(error::invalid_address);
	}

	if (!vsm::all_flags(get_max_protection(h), protection))
	{
		return vsm::unexpected(error::invalid_argument);
	}

	vsm_try(new_page_protection, get_page_protection(protection));

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
			h.size));
	}
	else
	{
		unrecoverable(unmap_view_of_section(
			GetCurrentProcess(),
			h.base));

		(void)close(h.section, a);
	}
	zero_native_handle(h);
	return {};
}
