#include <allio/win32/detail/iocp/wait.hpp>

#include <allio/impl/win32/kernel.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = platform_object_t;
using N = H::native_type;
using S = iocp_wait_state;

io_result2<void> iocp_wait_state::submit(M& m, N const& h, S& s)
{
	vsm_try(lease, m.lease_wait_packet(wait_packet));

	wait_slot.bind(s);

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
	lease.retain();

	return io_pending;
}

io_result2<void> iocp_wait_state::notify(M& m, N const& h, S& s, io_status const p_status)
{
	auto const& status = m.unwrap_io_status(p_status);
	vsm_assert(&status.slot == &wait_slot);

	m.release_wait_packet(vsm_move(wait_packet));

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	return {};
}

void iocp_wait_state::cancel(M& m, N const& h, S& s)
{
	m.cancel_wait(wait_packet.get());
	m.release_wait_packet(vsm_move(wait_packet));
}
