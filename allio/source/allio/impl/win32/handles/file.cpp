#include <allio/detail/handles/file.hpp>

#include <allio/impl/win32/byte_io.hpp>
#include <allio/impl/win32/handles/fs_object.hpp>
#include <allio/impl/win32/kernel.hpp>

#include <vsm/numeric.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

fs_path detail::get_null_device_path()
{
	return platform_path_view(L"\\??\\Device\\Null");
}

vsm::result<void> file_t::open(
	native_type& h,
	io_parameters_t<file_t, open_t> const& a)
{

	h = platform_object_t::native_type
	{
		object_t::native_type
		{
			flags::not_null | flags,
		},
		wrap_handle(directory.release()),
	};

	return {};
}

vsm::result<fs_size> file_t::tell(
	native_type const& h,
	io_parameters_t<file_t, tell_t> const&)
{
	FILE_POSITION_INFORMATION information;

	IO_STATUS_BLOCK io_status_block;
	NTSTATUS const status = NtQueryInformationFile(
		unwrap_handle(h.platform_handle),
		&io_status_block,
		&information,
		sizeof(information),
		FilePositionInformation);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return information.CurrentByteOffset.QuadPart;
}

vsm::result<void> file_t::seek(
	native_type const& h,
	io_parameters_t<file_t, seek_t> const& a)
{
	FILE_POSITION_INFORMATION information = {};
	information.CurrentByteOffset.QuadPart = a.offset;

	IO_STATUS_BLOCK io_status_block;
	NTSTATUS const status = NtSetInformationFile(
		unwrap_handle(h.platform_handle),
		&io_status_block,
		&information,
		sizeof(information),
		FilePositionInformation);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

vsm::result<size_t> file_t::stream_read(
	native_type const& h,
	io_parameters_t<file_t, stream_read_t> const& a)
{
	return win32::stream_read(h, a);
}

vsm::result<size_t> file_t::stream_write(
	native_type const& h,
	io_parameters_t<file_t, stream_write_t> const& a)
{
	return win32::stream_write(h, a);
}

vsm::result<size_t> file_t::random_read(
	native_type const& h,
	io_parameters_t<file_t, random_read_t> const& a)
{
	return win32::random_read(h, a);
}

vsm::result<size_t> file_t::random_write(
	native_type const& h,
	io_parameters_t<file_t, random_write_t> const& a)
{
	return win32::random_write(h, a);
}
