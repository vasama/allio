#include <allio/win32/detail/iocp/event_handle.hpp>

#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>

#include <vsm/defer.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = event_handle_t;
using C = connector_t<M, H>;

using wait_t = event_handle_t::wait_t;
using wait_s = operation_t<M, H, wait_t>;
using wait_r = io_result_ref_t<wait_t>;

io_result operation_impl<M, H, wait_t>::submit(M& m, H const& h, C const& c, wait_s& s, wait_r)
{
	if (!h)
	{
		return error::handle_is_null;
	}

	// Acquire wait packet.
	{
		auto r = m.acquire_wait_packet();

		if (!r)
		{
			return r.error();
		}

		s.wait_packet = *vsm_move(r);
	}

	bool release_wait_packet = true;
	vsm_defer
	{
		if (release_wait_packet)
		{
			m.release_wait_packet(vsm_move(s.wait_packet));
		}
	};

	s.wait_slot.set_operation(s);

	auto const r = m.submit_wait(
		s.wait_packet.get(),
		s.wait_slot,
		h.get_platform_handle());

	if (!r)
	{
		return r.error();
	}

	if (*r)
	{
		return std::error_code();
	}

	release_wait_packet = false;
	return std::nullopt;
}

io_result operation_impl<M, H, wait_t>::notify(M& m, H const& h, C const& c, wait_s& s, wait_r, io_status const status)
{
	auto const wait_status = M::unwrap_io_status(status);
	vsm_assert(&wait_status.slot == &s.wait_slot);

	m.release_wait_packet(vsm_move(s.wait_packet));
	return get_kernel_error_code(wait_status.status);
}

void operation_impl<M, H, wait_t>::cancel(M& m, H const& h, C const& c, S& s)
{
	(void)m.cancel_wait(s.wait_packet.get());
}
