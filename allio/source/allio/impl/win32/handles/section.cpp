#include <allio/detail/handles/section.hpp>

#include <allio/impl/win32/handles/platform_object.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/detail/unique_handle.hpp>
#include <allio/win32/kernel_error.hpp>
#include <allio/win32/platform.hpp>

#include <vsm/out_resource.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

static protection get_file_protection(fs_object_t::native_type const& h)
{
	protection protection = protection::none;
	if (h.flags[fs_object_t::flags::readable])
	{
		protection |= protection::read;
	}
	if (h.flags[fs_object_t::flags::writable])
	{
		protection |= protection::write;
	}
	return protection;
};

static ACCESS_MASK get_section_access(protection const protection)
{
	ACCESS_MASK section_access = STANDARD_RIGHTS_REQUIRED | SECTION_QUERY;
	if (vsm::any_flags(protection, protection::read))
	{
		section_access |= SECTION_MAP_READ | SECTION_MAP_EXECUTE;
	}
	if (vsm::any_flags(protection, protection::write))
	{
		section_access |= SECTION_MAP_WRITE;
	}
	if (vsm::any_flags(protection, protection::execute))
	{
		section_access |= SECTION_MAP_EXECUTE_EXPLICIT;
	}
	return section_access;
}

//TODO: Deduplicate with map.cpp
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

vsm::result<void> section_t::create(
	native_type& h,
	io_parameters_t<section_t, create_t> const& a)
{
	if (a.backing_file == nullptr)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	auto const& backing_file_h = *a.backing_file;
	if (!backing_file_h.flags[object_t::flags::not_null])
	{
		return vsm::unexpected(error::invalid_argument);
	}

	auto const maximum_protection = get_file_protection(backing_file_h);
	auto const default_protection = maximum_protection & protection::read_write;
	auto const protection = a.protection.value_or(default_protection);

	if (!vsm::all_flags(maximum_protection, protection))
	{
		return vsm::unexpected(error::invalid_argument);
	}

	LARGE_INTEGER max_size_integer;
	max_size_integer.QuadPart = a.max_size;

	auto const section_access = get_section_access(protection);
	vsm_try(page_protection, get_page_protection(protection));

	unique_handle section;
	NTSTATUS const status = NtCreateSection(
		vsm::out_resource(section),
		//TODO: Set section access flags
		section_access,
		/* ObjectAttributes: */ nullptr,
		&max_size_integer,
		page_protection,
		//TODO: Set section allocation attributes
		SEC_COMMIT,
		unwrap_handle(backing_file_h.platform_handle));

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	h = native_type
	{
		platform_object_t::native_type
		{
			object_t::native_type
			{
				flags::not_null,
			},
			wrap_handle(section.release()),
		},
		protection,
	};

	return {};
}
