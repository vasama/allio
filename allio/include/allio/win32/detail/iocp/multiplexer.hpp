#pragma once

#include <allio/async_result.hpp>
#include <allio/detail/io.hpp>
#include <allio/multiplexer.hpp>
#include <allio/win32/detail/handles/platform_handle.hpp>
#include <allio/win32/detail/unique_handle.hpp>
#include <allio/win32/detail/win32_fwd.hpp>
#include <allio/win32/wait_packet.hpp>

#include <vsm/intrusive_ptr.hpp>
#include <vsm/linear.hpp>

#include <bit>

namespace allio::detail {

//TODO: Allow setting the NumberOfConcurrentThreads and
//      initializing with another iocp_multiplexer to duplicate the handle.
class iocp_multiplexer final
{
public:
#if 0
	class operation
	{
		void(*m_io_callback)(iocp_multiplexer& m, operation& s, io_slot& slot) noexcept = nullptr;

	public:
		template<std::derived_from<operation> S>
		void set_io_handler(this S& self, auto handler)
		{
			static_assert(std::is_empty_v<decltype(handler)>);
			self.m_io_handler = [](iocp_multiplexer& m, operation& s, io_slot& slot) noexcept
			{
				std::bit_cast<decltype(handler)>(static_cast<unsigned char>(0))(m, static_cast<S&>(s), slot);
			};
		}

	private:
		friend class iocp_multiplexer;
	};
#endif

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
	
	static io_status_type& unwrap_io_status(io_status* const status)
	{
		return *reinterpret_cast<io_status_type*>(status);
	}

private:
	struct shared_state_t : vsm::intrusive_ref_count
	{
		unique_handle completion_port;
	};

	vsm::intrusive_ptr<shared_state_t> m_shared_state;
	vsm::linear<HANDLE> m_completion_port;

public:
	#define allio_iocp_multiplexer_create_parameters(type, data, ...) \
		type(::allio::detail::iocp_multiplexer, create_parameters) \
		data(::size_t, max_concurrent_threads, 1) \

	allio_interface_parameters(allio_iocp_multiplexer_create_parameters);

	template<parameters<create_parameters> P = create_parameters::interface>
	[[nodiscard]] static vsm::result<iocp_multiplexer> create(P const& args = {})
	{
		return _create(args);
	}

	[[nodiscard]] static vsm::result<iocp_multiplexer> create(iocp_multiplexer const& other)
	{
		return _create(other);
	}


	/// @return True if the multiplexer made any progress.
	template<parameters<poll_parameters> P = poll_parameters::interface>
	[[nodiscard]] vsm::result<bool> poll(P const& args = {})
	{
		return _poll(args);
	}


	//TODO: Take native_platform_handle instead of platform_handle.
	[[nodiscard]] vsm::result<void> attach(platform_handle const& h, connector_type& c);
	//TODO: Return a vsm::result<void> instead of void as detaching can fail.
	//      Handles should gain a multiplexer coupling aware close to avoid detaching.
	void detach(platform_handle const& h, connector_type& c, error_handler* e);


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


	[[nodiscard]] static bool supports_synchronous_completion(platform_handle const& h)
	{
		return h.get_flags()[platform_handle::impl_type::flags::skip_completion_port_on_success];
	}


private:
	explicit iocp_multiplexer(vsm::intrusive_ptr<shared_state_t> shared_state);


	[[nodiscard]] vsm::result<bool> _poll(poll_parameters const& args);

	[[nodiscard]] static vsm::result<iocp_multiplexer> _create(create_parameters const& args);
	[[nodiscard]] static vsm::result<iocp_multiplexer> _create(iocp_multiplexer const& other);
};

template<std::derived_from<platform_handle> H>
struct connector_impl<iocp_multiplexer, H> : iocp_multiplexer::connector_type
{
	using M = iocp_multiplexer;
	using C = connector_t<M, H>;

	friend vsm::result<void> tag_invoke(attach_handle_t, M& m, H const& h, C& c)
	{
		return m.attach(h, c);
	}

	friend void tag_invoke(detach_handle_t, M& m, H const& h, C& c)
	{
		return m.detach(h, c);
	}
};

} // namespace allio::detail
