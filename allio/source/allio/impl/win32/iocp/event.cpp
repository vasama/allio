#include <allio/win32/detail/iocp/event.hpp>

#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>

#include <vsm/defer.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = event_t;
using N = H::native_type;
using C = connector_t<M, H>;

using wait_t = event_t::wait_t;
using wait_i = operation<M, H, wait_t>;
using wait_s = operation_t<M, H, wait_t>;
using wait_a = io_parameters_t<wait_t>;

io_result<void> wait_i::submit(M& m, N const& h, C const& c, wait_s& s, wait_a const& args, io_handler<M>& handler)
{
	return s.wait_state.submit(m, h, s, handler);
}

io_result<void> wait_i::notify(M& m, N const& h, C const& c, wait_s& s, wait_a const& args, M::io_status_type const status)
{
	return s.wait_state.notify(m, h, s, status);
}

void wait_i::cancel(M& m, N const& h, C const& c, S& s)
{
	s.wait_state.cancel(m, h, s);
}
