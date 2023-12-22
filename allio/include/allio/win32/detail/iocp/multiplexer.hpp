#pragma once

#include <allio/detail/deadline.hpp>
#include <allio/detail/multiplexer.hpp>
#include <allio/detail/parameters.hpp>
#include <allio/detail/unique_handle.hpp>
#include <allio/win32/handles/platform_object.hpp>
#include <allio/win32/detail/wait_packet.hpp>
#include <allio/win32/detail/win32_fwd.hpp>

#include <vsm/intrusive_ptr.hpp>
#include <vsm/linear.hpp>
#include <vsm/result.hpp>

#include <bit>

namespace allio::detail {

namespace iocp {

struct max_concurrent_threads_t
{
	size_t max_concurrent_threads = 1;
};
inline constexpr explicit_parameter<max_concurrent_threads_t> max_concurrent_threads = {};

} // namespace iocp

class iocp_multiplexer final
{
public:
	using multiplexer_concept = void;

	struct io_status_type;

	class connector_type : public async_connector_base
	{
	};

	class operation_type : public async_operation_base
	{
	};

	class io_slot
	{
		using io_handler_type = basic_io_handler<io_status_type>;

		io_handler_type* m_handler = nullptr;

	public:
		void bind(io_handler_type& handler) &
		{
			m_handler = &handler;
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

	protected:
		alignas(uintptr_t) std::byte m_buffer[buffer_size];

	public:
		T* get()
		{
			return std::launder(reinterpret_cast<T*>(m_buffer));
		}

		T const* get() const
		{
			return std::launder(reinterpret_cast<T const*>(m_buffer));
		}

		T& operator*()
		{
			return *get();
		}

		T const& operator*() const
		{
			return *get();
		}

		T* operator->()
		{
			return get();
		}

		T const* operator->() const
		{
			return get();
		}
	};

	template<typename T>
	class basic_io_slot
		: public io_slot
		, public basic_io_slot_storage<T>
	{
		static consteval size_t get_storage_offset()
		{
			return offsetof(basic_io_slot, m_buffer);
		}

		friend class iocp_multiplexer;
	};

public:
	using io_status_block = basic_io_slot<IO_STATUS_BLOCK>;
	using overlapped = basic_io_slot<OVERLAPPED>;

	struct wait_slot : io_slot
	{
	};

	struct io_status_type
	{
		io_slot& slot;
		NTSTATUS status;
	};

private:
	struct shared_state_t : vsm::intrusive_ref_count
	{
		unique_handle completion_port;
	};

	// @brief The shared state owns the completion port object.
	//        Multiple iocp_multiplexers can share the same completion port.
	//        completion port handles cannot be duplicated,
	//        so the sharing must be implemented in user space.
	vsm::intrusive_ptr<shared_state_t> m_shared_state;

	/// @brief Non-owning copy of the completion port handle.
	vsm::linear<HANDLE> m_completion_port;


	using create_parameters = iocp::max_concurrent_threads_t;
	using poll_parameters = deadline_t;

public:
	[[nodiscard]] static vsm::result<iocp_multiplexer> create(auto&&... args)
	{
		return _create(make_args<create_parameters>(vsm_forward(args)...));
	}

	[[nodiscard]] static vsm::result<iocp_multiplexer> create(iocp_multiplexer const& other)
	{
		return _create(other);
	}


	/// @return True if the multiplexer made any progress.
	[[nodiscard]] vsm::result<bool> poll(auto&&... args)
	{
		return _poll(make_args<poll_parameters>(vsm_forward(args)...));
	}


	[[nodiscard]] vsm::result<void> attach_handle(native_platform_handle handle, connector_type& c);

	//TODO: Handles should gain a multiplexer coupling aware close to avoid detaching.
	[[nodiscard]] vsm::result<void> detach_handle(native_platform_handle handle, connector_type& c);


	/// @brief Attempt to cancel a pending I/O operation described by handle and slot.
	/// @return True if the operation was cancelled before its completion.
	/// @note A completion event is queued regardless of whether the operation was cancelled.
	[[nodiscard]] bool cancel_io(io_slot& slot, native_platform_handle handle);


	/// @brief Acquire a wait packet from the multiplexer's pool.
	/// @return Returns a unique wait packet handle.
	[[nodiscard]] vsm::result<unique_wait_packet> acquire_wait_packet();

	/// @brief Release a wait packet into the multiplexer's pool.
	void release_wait_packet(unique_wait_packet&& packet);

	/// @brief Represents a lease of a wait packet from a multiplexer.
	///        On destruction, if the lease is still active, the wait packet is returned to the multiplexer.
	class wait_packet_lease
	{
		struct deleter
		{
			iocp_multiplexer* m_multiplexer;

			void vsm_static_operator_invoke(unique_wait_packet* const wait_packet)
			{
				m_multiplexer->release_wait_packet(vsm_move(*wait_packet));
			}
		};
		std::unique_ptr<unique_wait_packet, deleter> m_wait_packet;

	public:
		explicit wait_packet_lease(iocp_multiplexer& multiplexer, unique_wait_packet& wait_packet)
			: m_wait_packet(&wait_packet, deleter{ &multiplexer })
		{
		}

		/// @brief Release lease on the wait_packet.
		///        The wait packet will not be returned to the multiplexer.
		void release()
		{
			(void)m_wait_packet.release();
		}
	};

	/// @brief Acquire a wait packet from the multiplexer's pool as a lease.
	/// @param wait_packet Reference to a unique wait packet handle to be assigned.
	/// @return Returns a leased wait packet. See @ref wait_packet_lease.
	[[nodiscard]] vsm::result<wait_packet_lease> lease_wait_packet(unique_wait_packet& wait_packet)
	{
		vsm_try_assign(wait_packet, acquire_wait_packet());
		return wait_packet_lease(*this, wait_packet);
	}

	/// @brief Submit a wait operation on the specified slot and handle, as if by WaitForSingleObject.
	/// @pre The wait_slot shall be associated with an operation state.
	/// @return True if the object was already signaled.
	/// @note A completion event is not queued if the object was already signaled.
	[[nodiscard]] vsm::result<bool> submit_wait(wait_packet packet, wait_slot& slot, native_platform_handle handle);

	/// @brief Attempt to cancel a wait operation started on the specified slot.
	/// @return True if the operation was cancelled before its completion.
	/// @note A completion event is queued regardless of whether the operation was cancelled.
	bool cancel_wait(wait_packet packet, wait_slot& slot);


	[[nodiscard]] static bool supports_synchronous_completion(native_handle<platform_object_t> const& h)
	{
		return h.flags[platform_object_t::impl_type::flags::skip_completion_port_on_success];
	}


	void external_synchronization_acquired()
	{
	}

	void external_synchronization_released()
	{
	}

private:
	explicit iocp_multiplexer(vsm::intrusive_ptr<shared_state_t> shared_state);

	[[nodiscard]] static vsm::result<iocp_multiplexer> _create(create_parameters const& args);
	[[nodiscard]] static vsm::result<iocp_multiplexer> _create(iocp_multiplexer const& other);


	friend vsm::result<void> tag_invoke(attach_handle_t, iocp_multiplexer& m, native_handle<platform_object_t> const& h, connector_type& c)
	{
		return m.attach_handle(h.platform_handle, c);
	}

	friend vsm::result<void> tag_invoke(detach_handle_t, iocp_multiplexer& m, native_handle<platform_object_t> const& h, connector_type& c)
	{
		return m.detach_handle(h.platform_handle, c);
	}


	[[nodiscard]] vsm::result<bool> _poll(poll_parameters const& args);

	friend vsm::result<bool> tag_invoke(poll_io_t, iocp_multiplexer& m, auto&&... args)
	{
		return m._poll(make_args<poll_parameters>(vsm_forward(args)...));
	}
};
static_assert(multiplexer<iocp_multiplexer>);

} // namespace allio::detail
