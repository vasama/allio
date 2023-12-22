#include <allio/win32/detail/iocp/wait.hpp>

#include <allio/impl/win32/kernel.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

io_result<void> iocp_wait_state::submit(M& m, H const& h, S& s, io_handler<M>& handler)
{
	vsm_try(lease, m.lease_wait_packet(wait_packet));

	wait_slot.bind(handler);

	vsm_try(already_signaled, m.submit_wait(
		wait_packet.get(),
		wait_slot,
		h.platform_handle));

	if (already_signaled)
	{
		return {};
	}

	// The wait is now pending so the wait packet must retained until completion.
	// Otherwise the lease automatically releases the wait packet back to the multiplexer.
	lease.release();

	return io_pending(error::async_operation_pending);
}

io_result<void> iocp_wait_state::notify(M& m, H const& h, S& s, M::io_status_type const status)
{
	vsm_assert(&status.slot == &wait_slot);

	m.release_wait_packet(vsm_move(wait_packet));

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	return {};
}

void iocp_wait_state::cancel(M& m, H const& h, S& s)
{
	m.cancel_wait(wait_packet.get(), wait_slot);
	m.release_wait_packet(vsm_move(wait_packet));
}
