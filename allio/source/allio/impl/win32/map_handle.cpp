#include <allio/map_handle.hpp>

#include <allio/win32/nt_error.hpp>
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

static vsm::result<void> close_mapping(void* base, size_t size, bool const has_backing_section)
{
	NTSTATUS const status = [&]() -> NTSTATUS
	{
		HANDLE const process = GetCurrentProcess();

		if (has_backing_section)
		{
			return NtUnmapViewOfSection(
				process,
				base);
		}
		else
		{
			return NtFreeVirtualMemory(
				process,
				&base,
				&size,
				MEM_RELEASE);
		}
	}();

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<nt_error>(status));
	}

	return {};
}


/// @return Maximum protection available on the section object.
static protection get_section_access(section_handle const& section)
{
	return section
		? section.get_protection()
		// Page file backed memory can use any protection.
		: protection::read_write | protection::execute;
}

vsm::result<void> map_handle_base::block_commit(void* base, size_t size, commit_parameters const& args) const
{
	if (!*this)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	if (m_section)
	{
		return vsm::unexpected(error::unsupported_operation);
	}

	if (!check_address_range(base, size))
	{
		return vsm::unexpected(error::invalid_address);
	}

	vsm_try(protection, [&]() -> vsm::result<protection>
	{
		protection const section_access = get_section_access(m_section);

		if (args.protection)
		{
			protection const protection = *args.protection;

			if (!vsm::all_flags(section_access, protection))
			{
				return vsm::unexpected(error::invalid_argument);
			}

			return protection;
		}

		return section_access & protection::read_write;
	}());

	vsm_try(page_protection, get_page_protection(protection));

	NTSTATUS const status = NtAllocateVirtualMemory(
		GetCurrentProcess(),
		&base,
		0, // ZeroBits,
		&size,
		MEM_COMMIT,
		page_protection);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<nt_error>(status));
	}

	return {};
}

vsm::result<void> map_handle_base::decommit(void* base, size_t size) const
{
	return vsm::unexpected(error::unsupported_operation);
}

vsm::result<void> map_handle_base::set_protection(void* base, size_t size, protection const protection) const
{
	if (!*this)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	if (!check_address_range(base, size))
	{
		return vsm::unexpected(error::invalid_address);
	}

	if (!vsm::all_flags(get_section_access(m_section), protection))
	{
		return vsm::unexpected(error::invalid_argument);
	}

	vsm_try(new_page_protection, get_page_protection(protection));

	ULONG old_page_protection;
	NTSTATUS const status = NtProtectVirtualMemory(
		GetCurrentProcess(),
		&base,
		&size,
		new_page_protection,
		&old_page_protection);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<nt_error>(status));
	}

	return {};
}


vsm::result<void> map_handle_base::sync_impl(io::parameters_with_result<io::map> const& args)
{
	vsm_try_void(kernel_init());


	map_handle& h = *args.handle;

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}


	section_handle const* const p_section = nullptr;

	if (p_section != nullptr && !*args.section)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	LARGE_INTEGER section_offset;
	LARGE_INTEGER* p_section_offset = nullptr;

	if (file_size const offset = args.section_offset)
	{
		if (p_section == nullptr)
		{
			return vsm::unexpected(error::invalid_argument);
		}

		section_offset.QuadPart = offset;
		p_section_offset = &section_offset;
	}

	page_level const page_level = args.page_level
		? *args.page_level
		: get_default_page_level();

	vsm_try(page_protection, get_page_protection(args.protection));
	vsm_try(allocation_type, get_page_level_allocation_type(page_level));

	allocation_type |= MEM_RESERVE;

	if (args.commit)
	{
		if (p_section == nullptr)
		{
			allocation_type |= MEM_COMMIT;
		}
	}

	void* base = reinterpret_cast<void*>(args.address);
	size_t size = args.size;

	section_handle section;

	if (p_section != nullptr)
	{
		vsm_try_void(section.duplicate(*args.section));
	}


	NTSTATUS const status = [&]() -> NTSTATUS
	{
		HANDLE const process = GetCurrentProcess();

		if (p_section != nullptr)
		{
			return NtMapViewOfSection(
				unwrap_handle(section.get_platform_handle()),
				process,
				&base,
				0, // ZeroBits
				size,
				p_section_offset,
				&size,
				ViewUnmap,
				allocation_type,
				page_protection);
		}
		else
		{
			return NtAllocateVirtualMemory(
				process,
				&base,
				0, // ZeroBits
				&size,
				allocation_type,
				page_protection);
		}
	}();

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<nt_error>(status));
	}

	vsm_defer
	{
		if (base != nullptr)
		{
			vsm_verify(close_mapping(base, size, p_section != nullptr));
		}
	};


	h.base_type::set_native_handle(
	{
		flags::not_null,
	});
	h.m_section = vsm_move(section);
	h.m_base.value = std::exchange(base, nullptr);
	h.m_size.value = size;
	h.m_page_level.value = page_level;

	return {};
}

vsm::result<void> map_handle_base::sync_impl(io::parameters_with_result<io::close> const& args)
{
	map_handle& h = static_cast<map_handle&>(*args.handle);

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	vsm_try_void(close_mapping(
		h.m_base.value,
		h.m_size.value,
		static_cast<bool>(h.m_section)));

	if (h.m_section)
	{
		unrecoverable(h.m_section.close());
	}

	return {};
}

allio_handle_implementation(map_handle);
