#include <allio/win32/detail/iocp/directory.hpp>

#include <allio/impl/error_encoding.hpp>
#include <allio/impl/win32/handles/directory.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = native_handle<directory_t>;
using C = async_connector_t<M, directory_t>;

vsm::result<void> C::attach(M& m, H const& h, C& c)
{
	return m.attach_handle(h, c, iocp_multiplexer::attachment_flags::use_apc_context);
}


using open_t = directory_t::open_t;
using open_s = async_operation<iocp_multiplexer, directory_t, open_t>;
using open_a = io_parameters_t<directory_t, open_t>;

io_result<void> open_s::submit(M& m, H& h, C& c, open_s&, open_a const& a, io_handler<M>&)
{
	basic_detached_handle<directory_t> handle;
	vsm_try_void(blocking_io<directory_t::open_t>(handle, a));

	vsm_try_void(m.attach_platform_handle(
		handle.native().platform_handle,
		c,
		iocp_multiplexer::attachment_flags::use_apc_context));

	h = handle.release();

	return {};
}

io_result<void> open_s::notify(M&, H&, C&, open_s&, open_a const&, io_handler<M>&, M::io_status_type)
{
	vsm_unreachable();
}

void open_s::cancel(M&, H const&, C const&, S&, A const&, io_handler<M> const&)
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
	IO_STATUS_BLOCK& io_status_block = s.io_status_block.initialize();

	NTSTATUS const status = query_directory_file_start(
		unwrap_handle(h.platform_handle),
		/* event: */ NULL,
		a.buffer,
		/* restart: */ false,
		/* apc_routine: */ nullptr,
		iocp_multiplexer::make_apc_context(handler),
		io_status_block);

	if (status != STATUS_PENDING && m.supports_synchronous_completion(h))
	{
		vsm_assert(io_status_block.Status == status);
		return read_completed(a.buffer, io_status_block);
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
	auto& io_status_block = s.io_status_block.get_existing();
	vsm_assert(status.status == io_status_block.Status);
	return read_completed(a.buffer, io_status_block);
}

void read_s::cancel(M& m, H const& h, C const&, S& s, A const&, io_handler<M> const&)
{
	(void)m.cancel_io(h.platform_handle, s.io_status_block);
}
