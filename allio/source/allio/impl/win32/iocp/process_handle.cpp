#include <allio/impl/win32/handles/process_handle.hpp>
#include <allio/win32/detail/iocp/process_handle.hpp>

#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/platform.hpp>

#include <vsm/defer.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = _process_handle;
using C = connector_t<M, H>;

using wait_t = _process_handle::wait_t;
using wait_s = operation_t<M, H, wait_t>;
using wait_r = io_result_ref_t<wait_t>;

static io_result handle_result(H const& h, wait_r const result, io_result const wait_result)
{
	if (!wait_result || *wait_result)
	{
		return wait_result;
	}

	return vsm::as_error_code(result.produce(
		get_process_exit_code(unwrap_handle(h.get_platform_handle()))
	));
}

io_result operation_impl<M, H, wait_t>::submit(M& m, H const& h, C const& c, wait_s& s, wait_r const r)
{
	return handle_result(h, r, s.wait_state.submit(m, h, s));
}

io_result operation_impl<M, H, wait_t>::notify(M& m, H const& h, C const& c, wait_s& s, wait_r const r, io_status const status)
{
	return handle_result(h, r, s.wait_state.notify(m, h, s, status));
}

void operation_impl<M, H, wait_t>::cancel(M& m, H const& h, C const& c, S& s)
{
	s.wait_state.cancel(m, h, s);
}
