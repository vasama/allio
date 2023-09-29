#include <allio/win32/detail/iocp/event_handle.hpp>

#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>

#include <vsm/defer.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = _event_handle;
using C = connector_t<M, H>;

static std::error_code get_kernel_error_code(NTSTATUS const status)
{
	return NT_SUCCESS(status)
		? std::error_code()
		: std::error_code(static_cast<kernel_error>(status));
}

using wait_t = _event_handle::wait_t;
using wait_s = operation_t<M, H, wait_t>;

submit_result operation_impl<M, H, wait_t>::submit(M& m, H const& h, C const& c, wait_s& s)
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

submit_result operation_impl<M, H, wait_t>::notify(M& m, H const& h, C const& c, wait_s& s, io_status* const status)
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
