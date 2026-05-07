#pragma once

#include <allio/detail/deadline.hpp>
#include <allio/detail/multiplexer.hpp>
#include <allio/detail/parameters.hpp>
#include <allio/detail/unique_handle.hpp>
#include <allio/win32/handles/platform_object.hpp>
#include <allio/win32/detail/wait_packet.hpp>
#include <allio/win32/detail/win32_fwd.hpp>

#include <vsm/intrusive_ptr.hpp>
#include <vsm/pointer_tag_pair.hpp>
#include <vsm/result.hpp>

#include <bit>

namespace allio::detail {

namespace iocp {

struct max_concurrent_threads_t : explicit_argument<max_concurrent_threads_t, size_t> {};
inline constexpr explicit_parameter<max_concurrent_threads_t> max_concurrent_threads{};

} // namespace iocp

class iocp_multiplexer final
{
public:
	using multiplexer_concept = void;

	struct connector_type {};
	struct operation_type {};
	struct io_status_type;

public:
	struct io_status_type
	{
		NTSTATUS status;
		uint32_t user_tag;
		uint64_t information;
	};

	struct alignas(8) io_handler_type : basic_io_handler<io_status_type>
	{
		using basic_io_handler::basic_io_handler;
	};

private:
	static constexpr uintptr_t max_user_data_tag = alignof(io_handler_type) - 1;

	using io_handler_ptr = vsm::pointer_tag_pair_with_max<
		io_handler_type,
		uint32_t,
		max_user_data_tag>;

	class basic_io_status_block_handler
	{
	public:
		io_handler_ptr m_handler;
	};

	template<typename IoStatusBlock>
	class basic_io_status_block
	{
		static constexpr size_t storage_size = win32_type_traits<IoStatusBlock>::size;
		static_assert(storage_size >= win32_type_traits<IO_STATUS_BLOCK>::size);

		alignas(void*) unsigned char m_storage[storage_size];

	public:
		[[nodiscard]] IoStatusBlock& initialize()
		{
			static_assert(std::is_trivially_default_constructible_v<IoStatusBlock>);
			return *::new (m_storage) IoStatusBlock;
		}

		[[nodiscard]] IoStatusBlock& get_existing()
		{
			return *std::launder(reinterpret_cast<IoStatusBlock*>(m_storage));
		}

		[[nodiscard]] IoStatusBlock const& get_existing() const
		{
			return *std::launder(reinterpret_cast<IoStatusBlock const*>(m_storage));
		}

	private:
		friend iocp_multiplexer;
	};

	template<typename IoStatusBlock>
	class basic_io_status_block_with_context
		: basic_io_status_block_handler
		, public basic_io_status_block<IoStatusBlock>
	{
	public:
		void set_context(io_handler_type& handler) &
		{
			m_handler = io_handler_ptr(&handler, 0);
		}

		void set_context(io_handler_type& handler, uintptr_t const user_tag) &
		{
			m_handler = io_handler_ptr(&handler, user_tag);
		}

	private:
		friend iocp_multiplexer;
	};

public:
	using io_status_block = basic_io_status_block<IO_STATUS_BLOCK>;
	using io_status_block_with_handler = basic_io_status_block_with_context<IO_STATUS_BLOCK>;
	using overlapped_with_handler = basic_io_status_block_with_context<OVERLAPPED>;

	enum class attachment_flags : uintptr_t
	{
		none                            = 0,

		use_apc_context                 = 1 << 0,
	};
	vsm_flag_enum_friend(attachment_flags);

private:
	struct linear_handle
	{
		HANDLE value = NULL;

		explicit linear_handle(HANDLE const value)
			: value(value)
		{
		}

		linear_handle(linear_handle&& other)
			: value(other.value)
		{
			other.value = NULL;
		}

		linear_handle& operator=(linear_handle&& other) &
		{
			HANDLE local = other.value;
			other.value = NULL;
			std::swap(value, local);
		}
	};

	struct shared_state_t : vsm::intrusive_refcount
	{
		unique_handle completion_port;
	};

	/// @brief The shared state owns the completion port object. Multiple iocp_multiplexers can
	///        share the same completion port. Completion port handles cannot be duplicated, so the
	///        sharing must be implemented in user space.
	vsm::intrusive_ptr<shared_state_t> m_shared_state;

	/// @brief Non-owning copy of the completion port handle.
	linear_handle m_completion_port;


	struct create_parameters
	{
		size_t max_concurrent_threads;
	};

	using poll_parameters = deadline_t;

public:
	[[nodiscard]] static vsm::result<iocp_multiplexer> create(auto&&... args)
	{
		auto a = create_parameters{};
		a.max_concurrent_threads = 1;
		(set_argument(a, vsm_forward(args)), ...);
		return _create(a);
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


	[[nodiscard]] vsm::result<void> attach_platform_handle(
		native_platform_handle handle,
		connector_type& c,
		attachment_flags flags = attachment_flags::none);

	//TODO: Handles should gain a multiplexer coupling aware close to avoid detaching.
	[[nodiscard]] vsm::result<void> detach_platform_handle(
		native_platform_handle handle,
		connector_type& c);

	template<platform_object Object>
	[[nodiscard]] vsm::result<void> attach_handle(
		native_handle<Object> const& h,
		async_connector<iocp_multiplexer, Object>& c,
		attachment_flags const flags = attachment_flags::none)
	{
		return _attach_handle(h, c, flags);
	}

	template<platform_object Object>
	[[nodiscard]] vsm::result<void> detach_handle(
		native_handle<Object> const& h,
		async_connector<iocp_multiplexer, Object>& c)
	{
		using platform_object_native_handle [[maybe_unused]] = native_handle<platform_object_t>;
		return detach_platform_handle(h.platform_object_native_handle::platform_handle, c);
	}


	[[nodiscard]] static void* make_apc_context(io_handler_type& handler)
	{
		return io_handler_ptr(&handler, 0).tagged_pointer();
	}

	[[nodiscard]] static void* make_apc_context(io_handler_type& handler, uint32_t const user_tag)
	{
		return io_handler_ptr(&handler, user_tag).tagged_pointer();
	}


	/// @brief Attempt to cancel a pending I/O operation described by handle and APC context.
	/// @return True if the operation was canceled before its completion.
	/// @note A completion event is queued regardless of whether the operation was canceled.
	template<typename IoStatusBlock>
	[[nodiscard]] bool cancel_io(
		native_platform_handle const handle,
		basic_io_status_block<IoStatusBlock>& io_status_block)
	{
		return _cancel_io(handle, reinterpret_cast<IO_STATUS_BLOCK*>(io_status_block.m_storage));
	}


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

			void operator()(unique_wait_packet* const wait_packet) const
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

	/// @brief Submit a wait operation on the specified handle and with the specified APC context,
	///        as if by WaitForSingleObject.
	/// @return True if the object was already signaled.
	/// @note A completion event is not queued if the object was already signaled.
	[[nodiscard]] vsm::result<bool> submit_wait(
		wait_packet packet,
		native_platform_handle handle,
		io_handler_type& handler,
		uint32_t user_tag = 0);

	/// @brief Attempt to cancel a wait operation using the specified packet and with the specified
	///        APC context.
	/// @return True if the operation was canceled before its completion.
	/// @note A completion event is queued regardless of whether the operation was canceled.
	bool cancel_wait(wait_packet packet, io_handler_type const& handler, uint32_t user_tag = 0);


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


	[[nodiscard]] vsm::result<void> _attach_handle(
		native_handle<platform_object_t> const& h,
		connector_type& c,
		attachment_flags flags);

	[[nodiscard]] bool _cancel_io(native_platform_handle handle, IO_STATUS_BLOCK* io_status_block);


	[[nodiscard]] vsm::result<bool> _poll(poll_parameters const& args);

	friend vsm::result<bool> tag_invoke(poll_io_t, iocp_multiplexer& m, auto&&... args)
	{
		return m._poll(make_args<poll_parameters>(vsm_forward(args)...));
	}
};
static_assert(multiplexer<iocp_multiplexer>);

} // namespace allio::detail
