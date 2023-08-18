#include <allio/win32/detail/iocp/event_handle.hpp>

#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>

#include <vsm/lift.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = event_handle;

template<>
struct async_handle_impl<M, H>
{
#if 0
	using C = async_handle_storage<M, H>;

	static vsm::result<void> attach(M& m, H const& h, C& c) noexcept
	{

	}

	static vsm::result<void> detach(M& m, H const& h, C& c) noexcept
	{
	}
#endif
};
template struct async_handle_facade<M, H>;

template<>
struct async_operation_impl<M, H, H::wait_t>
{
	using C = async_handle_storage<M, H>;
	using S = async_operation_storage<M, H::base_type, H::wait_t>;

	static void io_handler(M& m, S& s, M::io_slot& slot) noexcept
	{
		vsm_assert(&slot == &s.wait_slot);

		s.set_result(NT_SUCCESS(s.wait_slot.Status)
			? std::error_code()
			: std::error_code(static_cast<kernel_error>(s.wait_slot.Status)));
	
		m.post(s, async_operation_status::concluded);
	}

	static vsm::result<void> submit(M& m, S& s) noexcept
	{
		return m.submit(s, [&]() -> vsm::result<void>
		{
			H const& h = *s.args.handle;
	
			if (!h)
			{
				return vsm::unexpected(error::handle_is_null);
			}

			s.set_io_handler(vsm_lift(io_handler));
			s.wait_slot.set_operation_storage(s);

			vsm_try(signaled, m.submit_wait(
				s.wait_slot,
				s.wait_packet,
				h.get_platform_handle()));

			if (signaled)
			{
				s.set_result(std::error_code());
				m.post(s, async_operation_status::concluded);
			}
	
			return {};
		});
	}

	static vsm::result<void> cancel(M& m, S& s) noexcept
	{
		vsm_try(cancelled, m.cancel_wait(
			s.wait_slot,
			s.wait_packet));
	
		if (cancelled)
		{
			s.set_result(error::async_operation_cancelled);
			m.post(s, async_operation_status::concluded);
		}
	
		return {};
	}
};
template struct async_operation_facade<M, H, H::wait_t>;
