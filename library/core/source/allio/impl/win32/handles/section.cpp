#include <allio/detail/handles/section.hpp>

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/win32/handles/platform_object.hpp>
#include <allio/impl/win32/memory.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>

#include <vsm/numeric.hpp>
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
	if (vsm::all_flags(protection, protection::read))
	{
		section_access |= SECTION_MAP_READ | SECTION_MAP_EXECUTE;
	}
	if (vsm::all_flags(protection, protection::write))
	{
		section_access |= SECTION_MAP_WRITE;
	}
	if (vsm::all_flags(protection, protection::execute))
	{
		section_access |= SECTION_MAP_EXECUTE_EXPLICIT;
	}
	return section_access;
}

static handle_flags make_protection_flags(protection const protection)
{
	handle_flags flags = handle_flags::none;
	if (vsm::all_flags(protection, protection::read))
	{
		flags |= section_t::flags::readable;
	}
	if (vsm::all_flags(protection, protection::write))
	{
		flags |= section_t::flags::writable;
	}
	if (vsm::all_flags(protection, protection::execute))
	{
		flags |= section_t::flags::executable;
	}
	return flags;
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
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		if (a.backing_storage == nullptr)
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		if (!a.backing_storage->flags[object_t::flags::not_null])
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		if (vsm::any_flags(a.options, section_options::backing_file))
		{
			backing_file_handle = unwrap_handle(a.backing_storage->platform_handle);

			maximum_protection = get_file_protection(*a.backing_storage);
			default_protection = maximum_protection;
		}
		else
		{
			//TODO: Open new backing file in the specified directory.
			return vsm::unexpected(allio_error(error::unsupported_operation));
		}
	}

	protection protection = default_protection;

	if (a.protection != detail::protection(0))
	{
		if (!vsm::all_flags(maximum_protection, a.protection))
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
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
	vsm_try_assign(max_size_integer.QuadPart, vsm::try_truncate<LONGLONG>(
		a.max_size,
		error::invalid_argument));

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
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

	h = native_handle<section_t>
	{
		native_handle<platform_object_t>
		{
			native_handle<object_t>
			{
				flags::not_null | make_protection_flags(protection),
			},
			wrap_handle(handle.release()),
		},
	};

	return {};
}
