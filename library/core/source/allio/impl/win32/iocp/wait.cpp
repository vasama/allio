#include <allio/win32/detail/iocp/wait.hpp>

#include <allio/impl/error_encoding.hpp>
#include <allio/impl/win32/kernel.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

io_result<void> iocp_wait_state::submit(M& m, H const& h, io_handler<M>& handler)
{
	vsm_try(lease, m.lease_wait_packet(wait_packet));

	vsm_try(already_signaled, m.submit_wait(
		wait_packet.get(),
		h.platform_handle,
		handler));

	if (already_signaled)
	{
		return {};
	}

	// The wait is now pending so the wait packet must retained until completion. Otherwise the
	// lease automatically releases the wait packet back to the multiplexer.
	lease.release();

	return vsm::unexpected(io_notify_status::submitted);
}

io_result<void> iocp_wait_state::notify(M& m, H const&, io_handler<M>& handler, M::io_status_type const status)
{
	m.release_wait_packet(vsm_move(wait_packet));

	if (!NT_SUCCESS(status.status))
	{
		if (status.status == STATUS_CANCELLED)
		{
			return vsm::unexpected(io_error_code(
				io_notify_status::cancelled,
				allio_error(static_cast<kernel_error>(status.status))));
		}

		return vsm::unexpected(allio_error(static_cast<kernel_error>(status.status)));
	}

	return {};
}

void iocp_wait_state::cancel(M& m, H const&, io_handler<M> const& handler)
{
	m.cancel_wait(wait_packet.get(), handler);
}
