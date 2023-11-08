#pragma once

#include <allio/detail/io.hpp>
#include <allio/multiplexer.hpp>
#include <allio/win32/handles/platform_handle.hpp>
#include <allio/win32/detail/unique_handle.hpp>
#include <allio/win32/detail/win32_fwd.hpp>
#include <allio/win32/wait_packet.hpp>

#include <vsm/intrusive_ptr.hpp>
#include <vsm/linear.hpp>
#include <vsm/result.hpp>

#include <bit>

namespace allio::detail {

namespace iocp {

struct max_concurrent_threads_t
{
	struct parameter_t
	{
		size_t max_concurrent_threads = 1;
	};

	struct argument_t
	{
		size_t max_concurrent_threads;

		friend void tag_invoke(set_argument_t, parameter_t& args, argument_t const& value)
		{
			args.max_concurrent_threads = value.max_concurrent_threads;
		}
	};

	argument_t vsm_static_operator_invoke(size_t const max_concurrent_threads)
	{
		return { max_concurrent_threads };
	}
};
inline constexpr max_concurrent_threads_t max_concurrent_threads;

} // namespace iocp

class iocp_multiplexer final
{
public:
	using multiplexer_tag = void;

	class connector_type
	{
	};

	class operation_type : public operation_base
	{
	protected:
		using operation_base::operation_base;
		operation_type(operation_type const&) = default;
		operation_type& operator=(operation_type const&) = default;
		~operation_type() = default;

	private:
		using operation_base::notify;

		friend class iocp_multiplexer;
	};

	class io_slot
	{
		operation_type* m_operation = nullptr;

	public:
		void set_operation(operation_type& operation) &
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

	protected:
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

	static io_status_type const& unwrap_io_status(io_status const status)
	{
		return status.unwrap<io_status_type>();
	}

private:
	// IOCP handles cannot be duplicated, so the sharing is implemented in user space.
	struct shared_state_t : vsm::intrusive_ref_count
	{
		unique_handle completion_port;
	};

	vsm::intrusive_ptr<shared_state_t> m_shared_state;
	vsm::linear<HANDLE> m_completion_port;


	struct create_t
	{
		using required_params_type = no_parameters_t;
		using optional_params_type = iocp::max_concurrent_threads_t::parameter_t;
	};

	struct poll_t
	{
		using required_params_type = no_parameters_t;
		using optional_params_type = deadline_t;
	};

public:
	[[nodiscard]] static vsm::result<iocp_multiplexer> create(auto&&... args)
	{
		return _create(io_args<create_t>()(vsm_forward(args)...));
	}

	[[nodiscard]] static vsm::result<iocp_multiplexer> create(iocp_multiplexer const& other)
	{
		return _create(other);
	}


	/// @return True if the multiplexer made any progress.
	[[nodiscard]] vsm::result<bool> poll(auto&&... args)
	{
		return _poll(io_args<poll_t>()(vsm_forward(args)...));
	}


	/// @brief Attempt to cancel a pending I/O operation described by handle and slot.
	/// @return True if the operation was cancelled before its completion.
	/// @note A completion event is queued regardless of whether the operation was cancelled.
	[[nodiscard]] bool cancel_io(io_slot& slot, native_platform_handle handle);


	/// @brief Acquire a wait packet from the multiplexer's pool.
	/// @return Returns a unique wait packet handle.
	[[nodiscard]] vsm::result<win32::unique_wait_packet> acquire_wait_packet();

	/// @brief Release a wait packet into the multiplexer's pool.
	void release_wait_packet(win32::unique_wait_packet&& packet);

	/// @brief Represents a lease of a wait packet from a multiplexer.
	///        On destruction, if the lease is still active, the wait packet is returned to the multiplexer.
	class wait_packet_lease
	{
		struct deleter
		{
			iocp_multiplexer* m_multiplexer;

			void vsm_static_operator_invoke(win32::unique_wait_packet* const wait_packet)
			{
				m_multiplexer->release_wait_packet(vsm_move(*wait_packet));
			}
		};
		vsm::unique_resource<win32::unique_wait_packet*, deleter> m_wait_packet;

	public:
		explicit wait_packet_lease(iocp_multiplexer& multiplexer, win32::unique_wait_packet& wait_packet)
			: m_wait_packet(&wait_packet, deleter{ &multiplexer })
		{
		}

		/// @brief Retain the leased wait_packet.
		///        The wait packet will not be returned to the multiplexer.
		void retain()
		{
			(void)m_wait_packet.release();
		}
	};

	/// @brief Acquire a wait packet from the multiplexer's pool as a lease.
	/// @param wait_packet Reference to a unique wait packet handle to be assigned.
	/// @return Returns a leased wait packet. See @ref wait_packet_lease.
	[[nodiscard]] vsm::result<wait_packet_lease> lease_wait_packet(win32::unique_wait_packet& wait_packet)
	{
		vsm_try_assign(wait_packet, acquire_wait_packet());
		return wait_packet_lease(*this, wait_packet);
	}

	/// @brief Submit a wait operation on the specified slot and handle, as if by WaitForSingleObject.
	/// @pre The wait_slot shall be associated with an operation state.
	/// @return True if the object was already signaled.
	/// @note A completion event is not queued if the object was already signaled.
	[[nodiscard]] vsm::result<bool> submit_wait(win32::wait_packet packet, wait_slot& slot, native_platform_handle handle);

	/// @brief Attempt to cancel a wait operation started on the specified slot.
	/// @return True if the operation was cancelled before its completion.
	/// @note A completion event is queued regardless of whether the operation was cancelled.
	[[nodiscard]] bool cancel_wait(win32::wait_packet packet);


	[[nodiscard]] static bool supports_synchronous_completion(platform_handle_t::native_type const& h)
	{
		return h.flags[platform_handle_t::impl_type::flags::skip_completion_port_on_success];
	}


private:
	explicit iocp_multiplexer(vsm::intrusive_ptr<shared_state_t> shared_state);

	[[nodiscard]] static vsm::result<iocp_multiplexer> _create(io_parameters_t<create_t> const& args);
	[[nodiscard]] static vsm::result<iocp_multiplexer> _create(iocp_multiplexer const& other);


	//TODO: Take native_platform_handle instead of platform_handle.
	[[nodiscard]] vsm::result<void> _attach_handle(native_platform_handle handle, connector_type& c);
	//TODO: Return a vsm::result<void> instead of void as detaching can fail.
	//      Handles should gain a multiplexer coupling aware close to avoid detaching.
	[[nodiscard]] vsm::result<void> _detach_handle(native_platform_handle handle, connector_type& c);

	friend vsm::result<void> tag_invoke(attach_handle_t, iocp_multiplexer& m, platform_handle_t::native_type const& h, connector_type& c)
	{
		return m._attach_handle(h.platform_handle, c);
	}

	friend vsm::result<void> tag_invoke(detach_handle_t, iocp_multiplexer& m, platform_handle_t::native_type const& h, connector_type& c)
	{
		return m._detach_handle(h.platform_handle, c);
	}


	[[nodiscard]] vsm::result<bool> _poll(io_parameters_t<poll_t> const& args);

	friend vsm::result<bool> tag_invoke(poll_io_t, iocp_multiplexer& m, auto&&... args)
	{
		return m._poll(io_args<poll_t>()(vsm_forward(args)...));
	}
};

} // namespace allio::detail
