#include <allio/win32/detail/iocp/generic_wait.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

static std::error_code get_kernel_error_code(NTSTATUS const status)
{
	return NT_SUCCESS(status)
		? std::error_code()
		: std::error_code(static_cast<kernel_error>(status));
}

using M = iocp_multiplexer;
using H = platform_handle;
using S = wait_operation_state;

submit_result wait_operation_state::submit(M& m, H const& h, S& s)
{
	if (!h)
	{
		return error::handle_is_null;
	}

	auto const lease = m.lease_wait_packet(s.wait_packet);

	if (!lease)
	{
		return lease.error();
	}

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

	lease->keep_wait_packet();

	return std::nullopt;
}

submit_result wait_operation_state::notify(M& m, H const& h, S& s, io_status* const status)
{
	iocp_multiplexer::wait_packet_lease const lease(m, s.wait_packet);

	auto const wait_status = m.unwrap_io_status(status);
	vsm_assert(&wait_status.slot == &s.wait_slot);

	return get_kernel_error_code(wait_status.status);
}

void wait_operation_state::cancel(M& m, H const& h, S& s)
{
	(void)m.cancel_wait(s.wait_packet.get());
	m.release_wait_packet(vsm_move(s.wait_packet));
}
