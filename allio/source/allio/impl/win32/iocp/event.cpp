#include <allio/win32/detail/iocp/event.hpp>

#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = event_t::native_type;
using C = async_connector_t<M, event_t>;

using wait_t = event_io::wait_t;
using wait_s = async_operation<M, event_t, wait_t>;
using wait_a = io_parameters_t<event_t, wait_t>;

io_result<void> wait_s::submit(M& m, H const& h, C const&, wait_s& s, wait_a const&, io_handler<M>& handler)
{
	return s.wait_state.submit(m, h, s, handler);
}

io_result<void> wait_s::notify(M& m, H const& h, C const&, wait_s& s, wait_a const&, M::io_status_type const status)
{
	return s.wait_state.notify(m, h, s, status);
}

void wait_s::cancel(M& m, H const& h, C const&, S& s)
{
	s.wait_state.cancel(m, h, s);
}
