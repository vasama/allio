#include <allio/impl/win32/handles/process.hpp>
#include <allio/win32/detail/iocp/process.hpp>

#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/platform.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = process_t::native_type;
using C = async_connector_t<M, process_t>;

using wait_t = process_io::wait_t;
using wait_s = async_operation_t<M, process_t, wait_t>;
using wait_a = io_parameters_t<process_t, wait_t>;

io_result<process_exit_code> wait_s::submit(M& m, H const& h, C const&, wait_s& s, wait_a const&, io_handler<M>& handler)
{
	vsm_try_void(s.wait_state.submit(m, h, s, handler));
	return get_process_exit_code(unwrap_handle(h.platform_handle));
}

io_result<process_exit_code> wait_s::notify(M& m, H const& h, C const&, wait_s& s, wait_a const&, M::io_status_type const status)
{
	vsm_try_void(h, s.wait_state.notify(m, h, s, status));
	return get_process_exit_code(unwrap_handle(h.platform_handle));
}

void wait_s::cancel(M& m, H const& h, C const&, S& s)
{
	s.wait_state.cancel(m, h, s);
}
