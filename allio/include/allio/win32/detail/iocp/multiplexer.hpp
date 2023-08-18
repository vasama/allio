#pragma once

#include <allio/async_defer_list.hpp>
#include <allio/detail/async_io.hpp>
#include <allio/multiplexer.hpp>
#include <allio/win32/detail/handles/platform_handle.hpp>
#include <allio/win32/detail/unique_handle.hpp>
#include <allio/win32/detail/win32_fwd.hpp>
#include <allio/win32/wait_packet.hpp>

#include <bit>

namespace allio::detail {

//TODO: Allow setting the NumberOfConcurrentThreads and
//      initializing with another iocp_multiplexer to duplicate the handle.
class iocp_multiplexer final
{
public:
	class io_slot;

	class operation_storage : public async_defer_list::operation_storage
	{
		void(*m_io_handler)(iocp_multiplexer& m, operation_storage& s, io_slot& slot) noexcept = nullptr;

	public:
		template<std::derived_from<operation_storage> S>
		void set_io_handler(this S& self, auto handler)
		{
			self.m_io_handler = [](iocp_multiplexer& m, operation_storage& s, io_slot& slot) noexcept
			{
				std::bit_cast<decltype(handler)>(static_cast<unsigned char>(0))(m, static_cast<S&>(s), slot);
			};
		}

	private:
		friend class iocp_multiplexer;
	};

	class io_slot
	{
		operation_storage* m_operation = nullptr;

	public:
		void set_operation_storage(operation_storage& operation) &
		{
			m_operation = &operation;
		}

	protected:
		io_slot() = default;
		io_slot(io_slot const&) = default;
		io_slot& operator=(io_slot const&) = default;
		~io_slot() = default;

	private:
		friend class iocp_multiplexer;
	};

private:
	template<typename T>
	class basic_io_slot_storage
	{
		static constexpr size_t buffer_size = win32_type_traits<T>::size;
		static_assert(buffer_size >= win32_type_traits<IO_STATUS_BLOCK>::size);
		alignas(uintptr_t) std::byte m_buffer [buffer_size];

	public:
		T& operator*()
		{
			return reinterpret_cast<T&>(m_buffer);
		}

		T const& operator*() const
		{
			return reinterpret_cast<T const&>(m_buffer);
		}

		T* operator->()
		{
			return reinterpret_cast<T*>(m_buffer);
		}

		T const* operator->() const
		{
			return reinterpret_cast<T const*>(m_buffer);
		}
	};

	template<typename T>
	class basic_io_slot
		: public io_slot
		, public basic_io_slot_storage<T>
	{
	};

public:
	using io_status_block = basic_io_slot<IO_STATUS_BLOCK>;
	using overlapped = basic_io_slot<OVERLAPPED>;

	struct wait_slot : io_slot
	{
		NTSTATUS Status;
	};

private:
	unique_handle m_completion_port;
	async_defer_list m_defer_list;

public:
	class context_type
	{
	};


	/// @return True if the multiplexer made any progress.
	vsm::result<bool> poll(poll_parameters const& args = {});


	vsm::result<void> attach(context_type& c, platform_handle const& h);
	vsm::result<void> detach(context_type& c, platform_handle const& h);


	vsm::result<void> submit(operation_storage& s, auto&& submit)
	{
		return m_defer_list.submit(s, vsm_forward(submit));
	}


	/// @brief Attempt to cancel a pending I/O operation described by handle and slot.
	/// @return True if the operation was cancelled before its completion.
	/// @note A completion event is queued regardless of whether the operation was cancelled.
	vsm::result<bool> cancel_io(io_slot& slot, native_platform_handle handle);


	vsm::result<win32::unique_wait_packet> acquire_wait_packet();
	vsm::result<void> release_wait_packet(win32::unique_wait_packet& packet);

	/// @brief Submit a wait operation on the specified slot and handle, as if by WaitForSingleObject.
	/// @return True if the handle was already in a signaled state.
	/// @post If the function returns true, no completion event is produced for this operation.
	vsm::result<bool> submit_wait(wait_slot& slot, win32::unique_wait_packet& packet, native_platform_handle handle);

	/// @brief Attempt to cancel a wait operation started on the specified slot.
	/// @return True if the operation was cancelled before its completion.
	/// @post If the function returns true, no completion event is produced for this operation.
	vsm::result<bool> cancel_wait(wait_slot& slot, win32::unique_wait_packet& packet);


	void post(operation_storage& s, async_operation_status const status)
	{
		m_defer_list.post(s, status);
	}


	static bool supports_synchronous_completion(platform_handle const& h)
	{
		return h.get_flags()[platform_handle::impl_type::flags::skip_completion_port_on_success];
	}

private:
	bool flush(poll_statistics& statistics);
};

} // namespace allio::detail
