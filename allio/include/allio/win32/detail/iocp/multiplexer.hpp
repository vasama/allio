#pragma once

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
	class io_slot;

	class operation
	{
		void(*m_io_handler)(iocp_multiplexer& m, operation& s, io_slot& slot) noexcept = nullptr;

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

	class io_slot
	{
		operation* m_operation = nullptr;

	public:
		void set_operation(operation& operation)&
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
	struct shared_object : vsm::intrusive_ref_count
	{
		unique_handle completion_port;
	};

	vsm::intrusive_ptr<shared_object> m_shared_object;
	vsm::linear<HANDLE> m_completion_port;

public:
	class connector_type
	{
	};


	#define allio_iocp_multiplexer_create_parameters(type, data, ...) \
		type(::allio::detail::iocp_multiplexer, create_parameters) \
		data(::size_t, max_concurrent_threads, 1) \

	allio_interface_parameters(allio_iocp_multiplexer_create_parameters);

	template<parameters<create_parameters> P = create_parameters::interface>
	static vsm::result<iocp_multiplexer> create(P const& args = {})
	{
		return _create(args);
	}

	static vsm::result<iocp_multiplexer> create(iocp_multiplexer const& other)
	{
		return _create(other);
	}


	/// @return True if the multiplexer made any progress.
	vsm::result<bool> poll(poll_parameters const& args = {});


	[[nodiscard]] vsm::result<void> attach(platform_handle const& h, connector_type& c);
	void detach(platform_handle const& h, connector_type& c, error_handler* e);


	vsm::result<void> submit(operation& s, auto&& submit);


	/// @brief Attempt to cancel a pending I/O operation described by handle and slot.
	/// @return True if the operation was cancelled before its completion.
	/// @note A completion event is queued regardless of whether the operation was cancelled.
	vsm::result<bool> cancel_io(io_slot& slot, native_platform_handle handle);


	[[nodiscard]] vsm::result<win32::unique_wait_packet> acquire_wait_packet();
	[[nodiscard]] vsm::result<void> release_wait_packet(win32::unique_wait_packet& packet);

	/// @brief Submit a wait operation on the specified slot and handle, as if by WaitForSingleObject.
	[[nodiscard]] vsm::result<void> submit_wait(wait_slot& slot, win32::unique_wait_packet& packet, native_platform_handle handle);

	/// @brief Attempt to cancel a wait operation started on the specified slot.
	/// @return True if the operation was cancelled before its completion.
	[[nodiscard]] bool cancel_wait(wait_slot& slot, win32::unique_wait_packet& packet, error_handler* e);


	[[nodiscard]] static bool supports_synchronous_completion(platform_handle const& h)
	{
		return h.get_flags()[platform_handle::impl_type::flags::skip_completion_port_on_success];
	}


private:
	explicit iocp_multiplexer(vsm::intrusive_ptr<shared_object> shared_object);


	static vsm::result<iocp_multiplexer> _create(create_parameters const& args);
	static vsm::result<iocp_multiplexer> _create(iocp_multiplexer const& other);


	bool flush(poll_statistics& statistics);
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

	friend void tag_invoke(detach_handle_t, M& m, H const& h, C& c, error_handler* const e)
	{
		return m.detach(h, c, e);
	}
};

} // namespace allio::detail
