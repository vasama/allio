#pragma once

#include <allio/async_result.hpp>
#include <allio/deferring_multiplexer.hpp>
#include <allio/detail/assert.hpp>
#include <allio/detail/capture.hpp>
#include <allio/detail/concepts.hpp>
#include <allio/detail/linear.hpp>
#include <allio/multiplexer.hpp>
#include <allio/platform_handle.hpp>
#include <allio/win32/detail/unique_handle.hpp>
#include <allio/win32/detail/win32_fwd.hpp>
#include <allio/win32/platform.hpp>

#include <bitset>
#include <new>

namespace allio::win32 {

class iocp_multiplexer final : public deferring_multiplexer
{
	template<size_t Size>
	class object_cache
	{
		unique_handle m_objects[Size];
		std::bitset<Size> m_mask;

	public:
		HANDLE try_acquire();
		bool try_release(HANDLE handle);
	};

public:
	class io_slot;

	class async_operation_storage : public deferring_multiplexer::async_operation_storage
	{
	public:
		using deferring_multiplexer::async_operation_storage::async_operation_storage;

	protected:
		virtual void io_completed(iocp_multiplexer& multiplexer, io_slot& slot) = 0;

	private:
		friend class iocp_multiplexer;
	};

	class io_slot
	{
		async_operation_storage* m_operation = nullptr;

	public:
		void set_handler(async_operation_storage& operation)
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
		std::byte m_buffer alignas(uintptr_t) [buffer_size];
	
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

	class wait_data
	{
	public:
		NTSTATUS Status;

	private:
		HANDLE wait_packet;

		friend class iocp_multiplexer;
	};

	class timeout_data
	{
	public:
		NTSTATUS Status;

	private:
		HANDLE waitable_timer;
		HANDLE wait_packet;

		friend class iocp_multiplexer;
	};

public:
	using io_status_block = basic_io_slot<IO_STATUS_BLOCK>;
	using overlapped = basic_io_slot<OVERLAPPED>;

	using wait_slot = basic_io_slot<wait_data>;
	using timeout_slot = basic_io_slot<timeout_data>;

private:
	unique_handle const m_completion_port;

	object_cache<8> m_wait_packet_cache;

public:
	struct init_options
	{
	};

	class init_result
	{
		unique_handle completion_port;
		friend class iocp_multiplexer;
	};

	static result<init_result> init(init_options const& options = {});


	iocp_multiplexer(init_result&& resources);
	~iocp_multiplexer() override;


	type_id<multiplexer> get_type_id() const override;

	result<multiplexer_handle_relation const*> find_handle_relation(type_id<handle> handle_type) const override;


	result<void> register_native_handle(native_platform_handle handle);
	result<void> deregister_native_handle(native_platform_handle handle);


	using multiplexer::pump;
	result<statistics> pump(pump_parameters const& args) override;


	// Attempt to cancel a pending I/O operation described by handle and slot.
	// A completion event is queued regardless of whether the operation was cancelled.
	result<void> cancel_io(io_slot& slot, native_platform_handle handle);


	// Start a wait operation on the specified slot and handle, as if by WaitForSingleObject.
	// Returns true if the handle was already in a signaled state.
	// If the function returns true, no completion event is produced for this operation.
	result<bool> start_wait(wait_slot& slot, native_platform_handle handle);

	// Cancel a wait operation started on the specified slot.
	// Returns true if the operation was cancelled before its completion.
	// If the function returns true, no completion event is produced for this operation.
	result<bool> cancel_wait(wait_slot& slot);


	result<void> start_timeout(timeout_slot& slot, deadline deadline);

	void cancel_timeout(timeout_slot& slot);


	static bool supports_synchronous_completion(platform_handle const& handle);

private:
	class wait_packet_deleter
	{
		iocp_multiplexer* m_multiplexer;

	public:
		wait_packet_deleter(iocp_multiplexer& multiplexer)
			: m_multiplexer(&multiplexer)
		{
		}

		void operator()(HANDLE const handle) const
		{
			m_multiplexer->release_wait_packet(handle);
		}
	};
	using unique_wait_packet = detail::unique_resource<HANDLE, wait_packet_deleter, NULL>;

	result<unique_wait_packet> acquire_wait_packet();
	void release_wait_packet(HANDLE handle);


	typedef void io_completion_callback(iocp_multiplexer& multiplexer, io_slot& slot);

	static void handle_wait_completion(iocp_multiplexer& multiplexer, io_slot& slot);
	static void handle_timeout_completion(iocp_multiplexer& multiplexer, io_slot& slot);
};

} // namespace allio::win32

allio_API extern allio_TYPE_ID(allio::win32::iocp_multiplexer);
