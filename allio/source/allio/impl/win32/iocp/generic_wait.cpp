#include <allio/win32/detail/iocp/generic_wait.hpp>

#include <allio/impl/win32/kernel.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = platform_handle;
using S = wait_operation_state;

io_result wait_operation_state::submit(M& m, H const& h, S& s)
{
	if (!h)
	{
		return error::handle_is_null;
	}

	auto lease = m.lease_wait_packet(this->wait_packet);

	if (!lease)
	{
		return lease.error();
	}

	this->wait_slot.set_operation(s);

	auto const r = m.submit_wait(
		this->wait_packet.get(),
		this->wait_slot,
		h.get_platform_handle());

	if (!r)
	{
		return r.error();
	}

	if (*r)
	{
		return std::error_code();
	}

	lease->retain();

	return std::nullopt;
}

io_result wait_operation_state::notify(M& m, H const& h, S& s, io_status const status)
{
	iocp_multiplexer::wait_packet_lease const lease(m, this->wait_packet);

	auto const wait_status = m.unwrap_io_status(status);
	vsm_assert(&wait_status.slot == &this->wait_slot);

	return get_kernel_error_code(wait_status.status);
}

void wait_operation_state::cancel(M& m, H const& h, S& s)
{
	(void)m.cancel_wait(this->wait_packet.get());
	m.release_wait_packet(vsm_move(this->wait_packet));
}
