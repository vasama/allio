#include <allio/impl/win32/map_handle.hpp>

#include <allio/impl/bounded_vector.hpp>
#include <allio/impl/win32/kernel.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::win32;

std::span<page_level const> allio::get_supported_page_levels()
{
	using supported_page_levels = bounded_vector<page_level, 2>;

	static auto const levels = []() -> supported_page_levels
	{
		supported_page_levels levels;

		SYSTEM_INFO system_info;
		GetSystemInfo(&system_info);

		levels.try_push_back(get_page_level(system_info.dwPageSize));

		if (size_t const large_page_size = GetLargePageMinimum())
		{
			vsm_assert(system_info.dwPageSize < large_page_size);
			levels.push_back(get_page_level(large_page_size));
		}

		return levels;
	}();

	return levels;
}


static vsm::result<ULONG> get_page_protection(page_access const access)
{
	switch (access)
	{
	case page_access::none:
		return PAGE_NOACCESS;

	case page_access::read:
		return PAGE_READONLY;

	case page_access::read | page_access::write:
		return PAGE_READWRITE;

	case page_access::execute:
		return PAGE_EXECUTE;

	case page_access::execute | page_access::read:
		return PAGE_EXECUTE_READ;

	case page_access::execute | page_access::read | page_access::write:
		return PAGE_EXECUTE_READWRITE;
	}
	
	return vsm::unexpected(error::unsupported_operation);
}

static vsm::result<ULONG> get_page_level_allocation_type(page_level const level)
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

	if (supported_levels.size() > 0 && level == supported_levels.back())
	{
		return MEM_LARGE_PAGES;
	}

	return vsm::unexpected(error::unsupported_page_level);
}


static vsm::result<void> close_mapping(void* const base, size_t const size, bool const has_backing_section)
{
	HANDLE const process = GetCurrentProcess();

	NTSTATUS const status = has_backing_section
		? NtUnmapViewOfSection(
			process,
			h.m_base.value)
		: NtFreeVirtualMemory(
			process,
			h.m_base.value,
			h.m_size.value,
			MEM_RELEASE);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<nt_error>(status));
	}

	return {};
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

	vsm_try(protection, get_page_protection(access));

	void* area_base = base;
	size_t area_size = size;

	NTSTATUS const status = NtProtectVirtualMemory(
		GetCurrentProcess(),
		&area_base,
		&area_size,
		protection,
		nullptr);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<nt_error>(status));
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


	HANDLE section = NULL;

	LARGE_INTEGER section_offset;
	LARGE_INTEGER* p_section_offset = nullptr;

	ULONG allocation_type = 0;

	handle::flags handle_flags = {};


	bool const has_backing_section = args.section != nullptr;

	if (has_backing_section)
	{
		section_handle const& sh = *args.section;

		if (!sh)
		{
			return vsm::unexpected(error::invalid_argument);
		}

		section = unwrap_handle(sh.get_platform_handle());

		handle_flags |= map_handle::implementation::flags::backing_section;
	}

	if (file_size const offset = args.offset)
	{
		if (!has_backing_section)
		{
			return vsm::unexpected(error::invalid_argument);
		}

		section_offset.QuadPart = offset;
		p_section_offset = &section_offset;
	}

	vsm_try(page_level_allocation_type, get_page_level_allocation_type(args.page_level));
	allocation_type |= page_level_allocation_type;

	vsm_try(page_protection, get_page_protection(args.access));

	void* base = reinterpret_cast<void*>(args.address);
	size_t size = args.size;


	HANDLE const process = GetCurrentProcess();

	NTSTATUS const status = has_backing_section
		? NtMapViewOfSection(
			section,
			process,
			&base,
			0, // ZeroBits
			size,
			p_section_offset,
			&size,
			ViewUnmap,
			allocation_type,
			page_protection)
		: NtAllocateVirtualMemory(
			process,
			&base,
			0 // ZeroBits
			&view_size,
			allocation_type,
			page_protection);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<nt_error>(status));
	}

	vsm_defer
	{
		if (base != nullptr)
		{
			vsm_verify(close_mapping(base, size, has_backing_section));
		}
	};

	vsm_try_void(h.set_native_handle(
		map_handle::native_handle_type
		{
			handle::native_handle_type
			{
				handle::flags::not_null | handle_flags
			},
			base,
			size,
		}
	));

	base = nullptr;

	return {};
}

vsm::result<void> detail::map_handle_base::sync_impl(io::parameters_with_result<io::close> const& args)
{
	map_handle& h = static_cast<map_handle&>(*args.handle);

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	vsm_try(native, h.release_native_handle());

	return close_mapping(
		native.base,
		native.size,
		native.flags[map_handle::implementation::flags::backing_section]);
}
