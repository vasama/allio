#include <allio/win32/detail/iocp/pipe.hpp>

#include <allio/win32/detail/iocp/byte_io.hpp>

using namespace allio;
using namespace allio::detail;

using M = iocp_multiplexer;
using H = native_handle<pipe_t>;
using C = async_connector_t<M, pipe_t>;

vsm::result<void> C::attach(M& m, H const& h, C& c)
{
	return m.attach_handle(h, c, iocp_multiplexer::attachment_flags::use_apc_context);
}


using read_t = pipe_t::stream_read_t;
using read_s = async_operation_t<M, pipe_t, read_t>;
using read_a = io_parameters_t<pipe_t, read_t>;

io_result<read_result> read_s::submit(
	M&,
	H const& h,
	C const&,
	read_s& s,
	read_a const& a,
	io_handler<M>& handler)
{
	return s.byte_io.submit<read_t>(h, a, handler);
}

io_result<read_result> read_s::notify(
	M&,
	H const& h,
	C const&,
	read_s& s,
	read_a const& a,
	io_handler<M>& handler,
	M::io_status_type const status)
{
	return s.byte_io.notify<read_t>(h, a, handler, status);
}

void read_s::cancel(M& m, H const& h, C const&, read_s& s, A const&, io_handler<M> const&)
{
	s.byte_io.cancel(m, h);
}


using write_t = pipe_t::stream_write_t;
using write_s = async_operation_t<M, pipe_t, write_t>;
using write_a = io_parameters_t<pipe_t, write_t>;

io_result<size_t> write_s::submit(
	M&,
	H const& h,
	C const&,
	write_s& s,
	write_a const& a,
	io_handler<M>& handler)
{
	return s.byte_io.submit<write_t>(h, a, handler);
}

io_result<size_t> write_s::notify(
	M&,
	H const& h,
	C const&,
	write_s& s,
	write_a const& a,
	io_handler<M>& handler,
	M::io_status_type const status)
{
	return s.byte_io.notify<write_t>(h, a, handler, status);
}

void write_s::cancel(M& m, H const& h, C const&, write_s& s, A const&, io_handler<M> const&)
{
	s.byte_io.cancel(m, h);
}
