#include <allio/map_handle.hpp>

using namespace allio;
using namespace allio::win32;

page_size allio::get_supported_page_sizes()
{
	static page_size const value = []() -> page_size
	{
		size_t const size = GetLargePageMinimum();

		switch (size)
		{
		case 2 * 1024 * 1024:
			return page_size::_4KB | page_size::_2MB;
		}
		
		static_assert(vsm_arch_x86,
			"Defaults for other architectures require investigation.");

		return page_size::_4KB;
	}();
	return value;
}

vsm::result<page_size> select_page_size(page_size const flags)
{
	auto const mode = flags | page_size::mode_mask;

	if (mode == page_size::exact)
	{
		page_size const size = flags & page_size::size_mask;

		if (vsm::math::is_power_of_two(std::to_underlying(size)))
		{
			return size;
		}
		else
		{
			return vsm::unexpected(error::invalid_argument);
		}
	}
	else
	{
		page_size const size = flags & get_supported_page_sizes();

		if (size == page_size(0))
		{
			return vsm::unexpected(error::unsupported_page_size);
		}

		if (mode == page_size::largest)
		{
			static constexpr auto b = vsm::math::high_bit<std::underlying_t<page_size>>;
			return static_cast<page_size>(b >> std::countr_zero(std::to_underlying(size)));
		}
		else
		{
			return static_cast<page_size>(1 << std::countr_zero(std::to_underlying(size)));
		}
	}
}


static vsm::result<ULONG> get_page_protection(page_access const access)
{
	switch (access)
	{
	case page_access::none;
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

static vsm::result<ULONG> get_page_size_allocation_type(page_size const size)
{

	switch (size)
	{
	case page_size::automatic:
	case page_size::smallest:
		return 0;

	case page_size::largest:
		//TODO: Select depending on section, privileges, support?
		return MEM_LARGE_PAGES;

#if vsm_arch_x86
	case page_size::x86_4KB:
		return 0;

	case page_size::x86_2MB:
		return MEM_LARGE_PAGES;
#endif
	}

	return vsm::unexpected(error::unsupported_operation);
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
	vsm_try(page_size_allocation_type, get_page_size_allocation_type(args.page_size));

	ULONG const allocation_type = page_size_allocation_type;

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
