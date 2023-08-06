#include <allio/map_handle.hpp>

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


vsm::result<void> detail::map_handle_base::sync_impl(io::parameters_with_result<io::map> const& args)
{
	map_handle& h = *args.handle;

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	vsm_assert(args.section != nullptr);
	section_handle const& sh = *args.section;

	if (!sh)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	vsm_try(page_protection, get_page_protection(args.access));

	vsm_try(page_level_allocation_type, get_page_level_allocation_type(args.page_level));
	ULONG const allocation_type = page_level_allocation_type;

	LARGE_INTEGER section_offset;
	LARGE_INTEGER* const p_section_offset = [&]() -> LARGE_INTEGER*
	{
		if (file_size const offset = args.offset)
		{
			section_offset.QuadPart = offset;
			return &section_offset;
		}
		return nullptr;
	}();

	void* address = reinterpret_cast<void*>(args.address);
	size_t view_size = size;

	NTSTATUS const status = NtMapViewOfSection(
		unwrap_handle(sh.get_platform_handle()),
		GetCurrentProcess(),
		&address,
		0,
		size,
		p_section_offset,
		&view_size,
		ViewUnmap,
		allocation_type,
		page_protection);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<nt_error>(status));
	}

	h.m_base.value = address;
	h.m_size.value = view_size;

	return {};
}
