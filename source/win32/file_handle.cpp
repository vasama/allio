#include <allio/file_handle.hpp>

#include <allio/win32/kernel_error.hpp>

#include "api_string.hpp"
#include "filesystem_handle.hpp"
#include "kernel.hpp"

using namespace allio;
using namespace allio::win32;

result<void> detail::file_handle_base::open_sync(filesystem_handle const* const base, path_view const path, file_parameters const& args)
{
	allio_ASSERT(!*this);
	allio_TRY(file, create_file(base, path, args));
	return consume_platform_handle(*this, { args.handle_flags }, std::move(file));
}

static result<size_t> scatter_gather_at(file_offset offset, auto const buffers, HANDLE const handle, decltype(win32::NtReadFile) const syscall)
{
	allio_TRYV(kernel_init());

	size_t transferred = 0;
	for (auto const buffer : buffers)
	{
		LARGE_INTEGER offset_integer;
		offset_integer.QuadPart = offset;

		IO_STATUS_BLOCK isb = make_io_status_block();

		NTSTATUS status = syscall(
			handle,
			NULL,
			nullptr,
			nullptr,
			&isb,
			(PVOID)buffer.data(),
			buffer.size(),
			&offset_integer,
			0);

		if (status == STATUS_PENDING)
		{
			status = io_wait(handle, &isb, deadline());
		}

		if (status < 0)
		{
			return allio_ERROR(static_cast<kernel_error>(status));
		}

		offset += buffer.size();
		transferred += isb.Information;
	}
	return transferred;
}

result<size_t> detail::file_handle_base::read_at_sync(file_offset const offset, read_buffers const buffers)
{
	allio_ASSERT(*this);
	return scatter_gather_at(offset, buffers, unwrap_handle(get_platform_handle()), NtReadFile);
}

result<size_t> detail::file_handle_base::write_at_sync(file_offset const offset, write_buffers const buffers)
{
	allio_ASSERT(*this);
	return scatter_gather_at(offset, buffers, unwrap_handle(get_platform_handle()), NtWriteFile);
}
