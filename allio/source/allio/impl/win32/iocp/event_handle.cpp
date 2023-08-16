#include <allio/win32/iocp/event_handle.hpp>

namespace allio {

using namespace win32;

using m_t = iocp_multiplexer;
using h_t = event_handle;


static void tag_invoke(io_notify_cpo<m_t, h_t, h_t::wait_tag>, m_t& m, auto& s) noexcept
{
	s.set_result(NT_SUCCESS(s.slot->Status)
		? std::error_code()
		: std::error_code(static_cast<nt_error>(slot->Status)));

	m.post(s, async_operation_status::concluded);
}

static vsm::result<void> tag_invoke(io_submit_cpo<m_t, h_t, h_t::wait_tag>, m_t& m, auto& s) noexcept
{
	return m.submit(s, [&]() -> vsm::result<void>
	{
		h_t const& h = *s.args.handle;

		if (!h)
		{
			return vsm::unexpected(error::handle_is_null);
		}

		s.slot.set_handler(s);
		vsm_try(signaled, m.submit_wait(
			s.slot,
			s.wait_packet,
			h.get_platform_handle()));

		if (signaled)
		{
			s.set_result(std::error_code());
			m.post(s, async_operations_status::concluded);
		}

		return {};
	});
}

static vsm::result<void> tag_invoke(io_cancel_cpo<m_t, h_t, h_t::wait_tag>, m_t& m, auto& s) noexcept
{
	vsm_try(cancelled, m.cancel_wait(s.slot, s.wait_packet));

	if (cancelled)
	{
		s.set_result(error::async_operation_cancelled);
		m.post(s, async_operation_status::concluded);
	}

	return {};
}

template struct async_operation_impl<m_t, h_t, h_t::wait_tag>;

} // namespace allio
