#include <allio/win32/detail/iocp/directory.hpp>

#include <allio/impl/error_encoding.hpp>
#include <allio/impl/win32/handles/directory.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using open_t = directory_t::open_t;
using open_s = async_operation<iocp_multiplexer, directory_t, open_t>;
using open_a = io_parameters_t<directory_t, open_t>;

io_result<void> open_s::submit(M& m, H& h, C& c, open_s&, open_a const& a, io_handler<M>&)
{
	basic_detached_handle<directory_t> handle;
	vsm_try_void(blocking_io<directory_t::open_t>(handle, a));
	vsm_try_void(m.attach_platform_handle(handle.native().platform_handle, c));
	h = handle.release();

	return {};
}

io_result<void> open_s::notify(M&, H&, C&, open_s&, open_a const&, io_handler<M>&, M::io_status_type)
{
	vsm_unreachable();
}

void open_s::cancel(M&, H const&, C const&, S&)
{
	vsm_unreachable();
}


using read_t = directory_t::read_t;
using read_s = async_operation<iocp_multiplexer, directory_t, read_t>;
using read_a = io_parameters_t<directory_t, read_t>;

static vsm::result<basic_directory_stream_view<void>> read_completed(
	read_buffer const buffer,
	IO_STATUS_BLOCK& io_status_block)
{
	directory_stream_pointer stream_pointer;
	NTSTATUS const status = query_directory_file_completed(
		buffer,
		io_status_block.Status,
		io_status_block.Information,
		stream_pointer);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

	return basic_directory_stream_view<void>(stream_pointer);
}

io_result<basic_directory_stream_view<void>> read_s::submit(
	M& m,
	H& h,
	C&,
	read_s& s,
	read_a const& a,
	io_handler<M>& handler)
{
	s.io_status_block.bind(handler);

	NTSTATUS const status = query_directory_file_start(
		unwrap_handle(h.platform_handle),
		/* event: */ NULL,
		a.buffer,
		/* restart: */ false,
		/* apc_routine: */ nullptr,
		/* apc_context: */ nullptr,
		*s.io_status_block);

	if (status != STATUS_PENDING && m.supports_synchronous_completion(h))
	{
		vsm_assert(s.io_status_block->Status == status);
		return read_completed(a.buffer, *s.io_status_block);
	}

	return vsm::unexpected(io_notify_status::submitted);
}

io_result<basic_directory_stream_view<void>> read_s::notify(
	M&,
	H&,
	C&,
	read_s& s,
	read_a const& a,
	io_handler<M>& handler,
	M::io_status_type const status)
{
	vsm_assert(&status.slot == &s.io_status_block);
	vsm_assert(status.status == s.io_status_block->Status);
	return read_completed(a.buffer, *s.io_status_block);
}

void read_s::cancel(M&, H const& h, C const&, S& s)
{
	io_cancel(unwrap_handle(h.platform_handle), *s.io_status_block);
}
