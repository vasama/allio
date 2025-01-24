#include <allio/detail/handles/file.hpp>

#include <allio/impl/error_encoding.hpp>
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
	native_handle<file_t>& h,
	io_parameters_t<file_t, open_t> const& a)
{
	return open_fs_object(h, a, open_kind::file);
}

vsm::result<fs_size> file_t::tell(
	native_handle<file_t> const& h,
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
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

	return static_cast<fs_size>(information.CurrentByteOffset.QuadPart);
}

vsm::result<void> file_t::seek(
	native_handle<file_t> const& h,
	io_parameters_t<file_t, seek_t> const& a)
{
	FILE_POSITION_INFORMATION information = {};

	vsm_try_assign(information.CurrentByteOffset.QuadPart, vsm::try_truncate<LONGLONG>(
		a.offset,
		error::file_offset_out_of_range));

	IO_STATUS_BLOCK io_status_block;
	NTSTATUS const status = NtSetInformationFile(
		unwrap_handle(h.platform_handle),
		&io_status_block,
		&information,
		sizeof(information),
		FilePositionInformation);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

	return {};
}

vsm::result<fs_size> file_t::get_maximum_extent(
	native_handle<file_t> const& h,
	io_parameters_t<file_t, get_maximum_extent_t> const& a)
{
	FILE_STANDARD_INFORMATION information;

	IO_STATUS_BLOCK io_status_block;
	NTSTATUS const status = NtQueryInformationFile(
		unwrap_handle(h.platform_handle),
		&io_status_block,
		&information,
		sizeof(information),
		FileStandardInformation);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

	return information.EndOfFile.QuadPart;
}

vsm::result<void> file_t::set_maximum_extent(
	native_handle<file_t> const& h,
	io_parameters_t<file_t, set_maximum_extent_t> const& a)
{
	vsm_try(size, vsm::try_truncate<LONGLONG>(
		a.size,
		error::invalid_argument));

	// Set FileEndOfFileInformation
	{
		FILE_END_OF_FILE_INFORMATION information;
		information.EndOfFile.QuadPart = size;

		IO_STATUS_BLOCK io_status_block;
		NTSTATUS const status = NtSetInformationFile(
			unwrap_handle(h.platform_handle),
			&io_status_block,
			&information,
			sizeof(information),
			FileEndOfFileInformation);

		if (!NT_SUCCESS(status))
		{
			return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
		}
	}

	// Set FileAllocationInformation
	{
		FILE_ALLOCATION_INFORMATION information;
		information.AllocationSize.QuadPart = size;

		IO_STATUS_BLOCK io_status_block;
		NTSTATUS const status = NtSetInformationFile(
			unwrap_handle(h.platform_handle),
			&io_status_block,
			&information,
			sizeof(information),
			FileAllocationInformation);

		if (!NT_SUCCESS(status))
		{
			return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
		}
	}

	return {};
}

vsm::result<size_t> file_t::stream_read(
	native_handle<file_t> const& h,
	io_parameters_t<file_t, stream_read_t> const& a)
{
	return win32::stream_read(h, a);
}

vsm::result<size_t> file_t::stream_write(
	native_handle<file_t> const& h,
	io_parameters_t<file_t, stream_write_t> const& a)
{
	return win32::stream_write(h, a);
}

vsm::result<size_t> file_t::random_read(
	native_handle<file_t> const& h,
	io_parameters_t<file_t, random_read_t> const& a)
{
	return win32::random_read(h, a);
}

vsm::result<size_t> file_t::random_write(
	native_handle<file_t> const& h,
	io_parameters_t<file_t, random_write_t> const& a)
{
	return win32::random_write(h, a);
}
