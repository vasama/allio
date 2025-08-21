#pragma once

#include <allio/detail/handles/platform_object.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/thread_event.hpp>

namespace allio::win32 {

//TODO: Rename this or the header
class wsa_thread_overlapped
{
	thread_event m_event;
	thread_event::overlapped_t m_overlapped;

public:
	[[nodiscard]] static vsm::result<wsa_thread_overlapped> get()
	{
		vsm::result<wsa_thread_overlapped> r(vsm::result_value);
		if (auto r2 = thread_event::get())
		{
			r->m_event = vsm_move(*r2);
		}
		else
		{
			r = vsm::unexpected(r2.error());
		}
		return r;
	}

	[[nodiscard]] static vsm::result<wsa_thread_overlapped> get_for(
		detail::native_handle<detail::platform_object_t> const& h)
	{
		vsm::result<wsa_thread_overlapped> r(vsm::result_value);
		if (h.flags[detail::platform_object_t::impl_type::flags::overlapped])
		{
			if (auto r2 = thread_event::get())
			{
				r->m_event = vsm_move(*r2);
			}
			else
			{
				r = vsm::unexpected(r2.error());
			}
		}
		return r;
	}

	[[nodiscard]] vsm::result<void> wait(
		SOCKET const socket,
		deadline const deadline,
		DWORD* const out_transferred,
		DWORD* const out_flags)
	{
		NTSTATUS const status = m_event.wait_for_io(
			reinterpret_cast<HANDLE>(socket),
			m_overlapped,
			deadline);

		if (!NT_SUCCESS(status) || status == STATUS_TIMEOUT)
		{
			return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
		}

		if (!WSAGetOverlappedResult(
			socket,
			&m_overlapped,
			out_transferred,
			FALSE,
			out_flags))
		{
			return vsm::unexpected(allio_error(posix::get_last_socket_error()));
		}

		return {};
	}

	template<std::same_as<OVERLAPPED> Overlapped>
	[[nodiscard]] operator Overlapped*() &
	{
		if (m_event)
		{
			//TODO: Set event low bit? Maybe set it in thread_event itself.
			m_overlapped.Pointer = nullptr;
			m_overlapped.hEvent = m_event;
			return &m_overlapped;
		}

		return nullptr;
	}
};

} // namespace allio::win32
