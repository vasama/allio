#include <allio/detail/handles/section.hpp>

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/win32/handles/platform_object.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>

#include <vsm/out_resource.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

static protection get_file_protection(native_handle<fs_object_t> const& h)
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
	native_handle<section_t>& h,
	io_parameters_t<section_t, create_t> const& a)
{
	static constexpr auto storage_options =
		section_options::backing_file |
		section_options::backing_directory;

	unique_handle unique_backing_file;
	HANDLE backing_file_handle = NULL;

	protection maximum_protection = protection::read_write | protection::execute;
	protection default_protection = protection::read_write;

	if (vsm::any_flags(a.options, storage_options))
	{
		if (vsm::all_flags(a.options, storage_options))
		{
			return vsm::unexpected(error::invalid_argument);
		}

		if (a.backing_storage == nullptr)
		{
			return vsm::unexpected(error::invalid_argument);
		}

		if (vsm::any_flags(a.options, section_options::backing_file))
		{
			if (!a.backing_storage->flags[object_t::flags::not_null])
			{
				return vsm::unexpected(error::invalid_argument);
			}

			backing_file_handle = unwrap_handle(a.backing_storage->platform_handle);

			maximum_protection = get_file_protection(*a.backing_storage);
			default_protection = maximum_protection;
		}
		else
		{
			//TODO: Open new backing file
			return vsm::unexpected(error::unsupported_operation);
		}
	}

	protection protection = default_protection;

	if (a.protection != detail::protection(0))
	{
		if (!vsm::all_flags(maximum_protection, a.protection))
		{
			return vsm::unexpected(error::invalid_argument);
		}

		protection = a.protection;
	}

	ULONG const section_access = get_section_access(protection);
	vsm_try(page_protection, get_page_protection(protection));

	OBJECT_ATTRIBUTES object_attributes = {};
	object_attributes.Length = sizeof(object_attributes);
	OBJECT_ATTRIBUTES* p_object_attributes = nullptr;

	if (vsm::any_flags(a.flags, io_flags::create_inheritable))
	{
		object_attributes.Attributes |= OBJ_INHERIT;
		p_object_attributes = &object_attributes;
	}

	LARGE_INTEGER max_size_integer;
	max_size_integer.QuadPart = a.max_size;

	unique_handle handle;
	NTSTATUS const status = NtCreateSection(
		vsm::out_resource(handle),
		//TODO: Set section access flags
		section_access,
		p_object_attributes,
		&max_size_integer,
		page_protection,
		//TODO: Set section allocation attributes
		SEC_COMMIT,
		backing_file_handle);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	h.flags = flags::not_null;
	h.platform_handle = wrap_handle(handle.release());
	h.protection = protection;

	return {};
}
