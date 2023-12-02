#include <allio/section.hpp>

#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/platform_object.hpp>
#include <allio/win32/detail/unique_handle.hpp>
#include <allio/win32/kernel_error.hpp>
#include <allio/win32/platform.hpp>

#include <vsm/out_resource.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<void> section_t::create(
	native_type& h
	io_parameters_t<section_t, section_io::create_t> const& a)
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

	LARGE_INTEGER max_size_integer;
	max_size_integer.QuadPart = a.max_size;

	unique_handle section;
	NTSTATUS const status = NtCreateSection(
		vsm::out_resource(section),
		SECTION_ALL_ACCESS, //TODO: Access
		/* ObjectAttributes: */ nullptr,
		&max_size_integer,
		0, //TODO: Page protection
		0, //TODO: Allocation attributes
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
		a.protection,
	};

	return {};
}
