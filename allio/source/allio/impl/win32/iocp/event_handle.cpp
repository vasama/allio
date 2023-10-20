#include <allio/win32/detail/iocp/event_handle.hpp>

#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>

#include <vsm/defer.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = event_handle_t;
using N = H::native_type;
using C = connector_t<M, H>;

using wait_t = event_handle_t::wait_t;
using wait_s = operation_t<M, H, wait_t>;

io_result2<void> operation_impl<M, H, wait_t>::submit(M& m, N const& h, C const& c, wait_s& s)
{
	// Acquire wait packet.
	vsm_try_assign(s.wait_packet, m.acquire_wait_packet());

	bool release_wait_packet = true;
	vsm_defer
	{
		if (release_wait_packet)
		{
			m.release_wait_packet(vsm_move(s.wait_packet));
		}
	};

	s.wait_slot.set_operation(s);

	vsm_try(already_signaled, m.submit_wait(
		s.wait_packet.get(),
		s.wait_slot,
		h.platform_handle));

	if (already_signaled)
	{
		return {};
	}

	release_wait_packet = false;
	return io_pending;
}

io_result2<void> operation_impl<M, H, wait_t>::notify(M& m, N const& h, C const& c, wait_s& s, io_status const status)
{
	auto const wait_status = M::unwrap_io_status(status);
	vsm_assert(&wait_status.slot == &s.wait_slot);

	m.release_wait_packet(vsm_move(s.wait_packet));

	if (!NT_SUCCESS(wait_status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(wait_status.status));
	}

	return {};
}

void operation_impl<M, H, wait_t>::cancel(M& m, N const& h, C const& c, S& s)
{
	(void)m.cancel_wait(s.wait_packet.get());
}
