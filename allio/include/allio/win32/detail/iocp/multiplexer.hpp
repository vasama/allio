#pragma once

#include <allio/async_io.hpp>
//#include <allio/win32/platform_handle.hpp> //TODO: Move impl flags here.
#include <allio/win32/detail/unique_handle.hpp>
#include <allio/win32/detail/win32_fwd.hpp>

namespace allio::detail {

struct unique_wait_packet {};

//TODO: Allow setting the NumberOfConcurrentThreads and
//      initializing with another iocp_multiplexer to duplicate the handle.
class iocp_multiplexer final
{
public:
	class io_slot;

	class operation_storage
	{
		void(*m_io_handler)(iocp_multiplexer& m, S& s, io_slot& slot) noexcept = nullptr;

	public:
		template<auto& F, std::derived_from<operation_storage> S>
		void set_io_handler(this S& self) &
		{
			m_io_handler = [](iocp_multiplexer& m, operation_storage& s, io_slot& slot) noexcept
			{
				F(m, static_cast<S&>(s), slot);
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
		static constexpr size_t buffer_size = detail::win32_type_traits<T>::size;
		static_assert(buffer_size >= detail::win32_type_traits<detail::IO_STATUS_BLOCK>::size);
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
	using io_status_block = basic_io_slot<detail::IO_STATUS_BLOCK>;
	using overlapped = basic_io_slot<detail::OVERLAPPED>;

private:
	detail::unique_handle m_completion_port;

public:
	class handle_type
	{
	public:
		vsm::result<void> attach(iocp_multiplexer& m, platform_handle const& h);
		vsm::result<void> detach(iocp_multiplexer& m, platform_handle const& h);


		/// @brief Attempt to cancel a pending I/O operation described by handle and slot.
		/// @return True if the operation was cancelled before its completion.
		/// @note A completion event is queued regardless of whether the operation was cancelled.
		vsm::result<bool> cancel_io(iocp_multiplexer& m, io_slot& slot, native_platform_handle handle) const;
	
	
		/// @brief Submit a wait operation on the specified slot and handle, as if by WaitForSingleObject.
		/// @return True if the handle was already in a signaled state.
		/// @post If the function returns true, no completion event is produced for this operation.
		vsm::result<bool> submit_wait(iocp_multiplexer& m, io_slot& slot, unique_wait_packet& packet, native_platform_handle handle) const;
	
		/// @brief Attempt to cancel a wait operation started on the specified slot.
		/// @return True if the operation was cancelled before its completion.
		/// @post If the function returns true, no completion event is produced for this operation.
		vsm::result<bool> cancel_wait(iocp_multiplexer& m, io_slot& slot, unique_wait_packet& packet, native_platform_handle handle) const;


		static bool supports_synchronous_completion(platform_handle const& h)
		{
			return h.get_flags()[platform_handle::impl_type::flags::skip_completion_port_on_success];
		}
	};


	/// @return True if the multiplexer made any progress.
	vsm::result<bool> poll(poll_parameters const& args = {});
};

} // namespace allio::detail
