#pragma once

#include <allio/detail/assert.hpp>
#include <allio/detail/capture.hpp>
#include <allio/detail/concepts.hpp>
#include <allio/detail/linear.hpp>
#include <allio/detail/unique_resource.hpp>
#include <allio/multiplexer.hpp>
#include <allio/platform_handle.hpp>
#include <allio/win32/platform.hpp>

#include <new>

struct _IO_STATUS_BLOCK
typedef IO_STATUS_BLOCK;

struct _OVERLAPPED
typedef OVERLAPPED;

namespace allio::win32 {

class iocp_multiplexer final : public multiplexer
{
public:
	enum class handle_flags : uintptr_t
	{
		none                                    = 0,
		supports_synchronous_completion         = 1 << 0,
	};
	allio_detail_FLAG_ENUM_FRIEND(handle_flags);

	static void* make_data(handle_flags const flags)
	{
		return reinterpret_cast<void*>(static_cast<uintptr_t>(flags));
	}

	static handle_flags get_flags(handle const& handle)
	{
		return static_cast<handle_flags>(reinterpret_cast<uintptr_t>(handle.get_multiplexer_data()));
	}

	static bool supports_synchronous_completion(handle_flags const flags)
	{
		return (flags & handle_flags::supports_synchronous_completion) != handle_flags::none;
	}


	class async_operation_storage;

	class io_slot
	{
		std::byte m_buffer alignas(uintptr_t) [sizeof(uintptr_t) * 3 + 8];
		async_operation_storage* m_storage = nullptr;

	public:
		IO_STATUS_BLOCK& io_status_block()
		{
			//return *std::launder(reinterpret_cast<IO_STATUS_BLOCK*>(m_buffer));
			return *reinterpret_cast<IO_STATUS_BLOCK*>(m_buffer);
		}

		IO_STATUS_BLOCK const& io_status_block() const
		{
			//return *std::launder(reinterpret_cast<IO_STATUS_BLOCK const*>(m_buffer));
			return *reinterpret_cast<IO_STATUS_BLOCK const*>(m_buffer);
		}

		OVERLAPPED& overlapped()
		{
			//return *std::launder(reinterpret_cast<OVERLAPPED*>(m_buffer));
			return *reinterpret_cast<OVERLAPPED*>(m_buffer);
		}

		OVERLAPPED const& overlapped() const
		{
			//return *std::launder(reinterpret_cast<OVERLAPPED const*>(m_buffer));
			return *reinterpret_cast<OVERLAPPED const*>(m_buffer);
		}

	private:
		static io_slot& get(IO_STATUS_BLOCK& io_status_block)
		{
			return reinterpret_cast<io_slot&>(io_status_block);
		}

		static io_slot const& get(IO_STATUS_BLOCK const& io_status_block)
		{
			return reinterpret_cast<io_slot const&>(io_status_block);
		}

		static io_slot& get(OVERLAPPED& overlapped)
		{
			return reinterpret_cast<io_slot&>(overlapped);
		}

		static io_slot const& get(OVERLAPPED const& overlapped)
		{
			return reinterpret_cast<io_slot const&>(overlapped);
		}

		friend class iocp_multiplexer;
	};

	class async_operation_storage : public async_operation
	{
		io_slot* m_slot = nullptr;
		async_operation_storage* m_next;

		result<void>(*m_capture_information)(async_operation_storage& storage, uintptr_t information) = nullptr;

	public:
		explicit async_operation_storage(async_operation_parameters const&, async_operation_listener* const listener)
			: async_operation(listener)
		{
		}

		template<typename Callable>
		void capture_information(Callable const callable)
		{
			allio_ASSERT(m_capture_information == nullptr);
			m_capture_information = detail::capture_traits<async_operation_storage, uintptr_t, decltype(&decltype(callable)::operator())>::callback;
		}

	private:
		result<void> set_information(uintptr_t const information)
		{
			if (m_capture_information != nullptr)
			{
				return m_capture_information(*this, information);
			}
			return {};
		}

		friend class iocp_multiplexer;
	};

	template<typename Operation>
	struct basic_async_operation_storage
		: async_operation_storage
		, io::parameters_with_result<Operation>
	{
		explicit basic_async_operation_storage(io::parameters_with_result<Operation> const& arguments, async_operation_listener* const listener)
			: async_operation_storage(arguments, listener)
			, io::parameters_with_result<Operation>(arguments)
		{
		}
	};

private:
	struct completion_port_deleter
	{
		static void release(HANDLE completion_port);

		void operator()(HANDLE const completion_port) const
		{
			release(completion_port);
		}
	};
	using unique_completion_port = detail::unique_resource<HANDLE, completion_port_deleter>;

	HANDLE m_completion_port;

	io_slot* m_slots;
	size_t m_slot_count;

	uint32_t* m_slot_ring;
	size_t m_slot_ring_head;
	size_t m_slot_ring_tail;

	async_operation_storage* m_synchronous_completion_head = nullptr;
	async_operation_storage* m_synchronous_completion_tail = nullptr;

public:
	struct init_options
	{
		uint32_t thread_count;
		size_t min_concurrent_operations;
	};

	class init_result
	{
		unique_completion_port completion_port;

		std::unique_ptr<io_slot[]> slots;
		std::unique_ptr<uint32_t[]> slot_ring;
		size_t slot_count;

		friend class iocp_multiplexer;
	};

	[[nodiscard]] static result<init_result> init(init_options const& options);


	iocp_multiplexer(init_result&& resources);
	iocp_multiplexer(iocp_multiplexer const&) = delete;
	iocp_multiplexer& operator=(iocp_multiplexer const&) = delete;
	~iocp_multiplexer();


	type_id<multiplexer> get_type_id() const override;

	result<multiplexer_handle_relation const*> find_handle_relation(type_id<handle> handle_type) const override;

	result<void> submit(deadline deadline) override;
	result<void> poll(deadline deadline) override;
	result<void> submit_and_poll(deadline deadline) override;


	result<handle_flags> register_native_handle(native_platform_handle handle);
	result<void> deregister_native_handle(native_platform_handle handle);


	class io_slot_list
	{
		iocp_multiplexer* m_multiplexer;
		detail::linear<size_t> m_index;
		detail::linear<size_t> m_count;

	public:
		explicit io_slot_list(iocp_multiplexer* const multiplexer, size_t const index, size_t const count)
			: m_multiplexer(multiplexer), m_index(index), m_count(count)
		{
		}

		io_slot_list(io_slot_list&&) = default;
		io_slot_list& operator=(io_slot_list&&) = default;

		~io_slot_list()
		{
			if (m_count.value > 0)
			{
				m_multiplexer->release_io_slots_internal(m_count.value);
			}
		}

		io_slot& bind(async_operation_storage& storage)
		{
			allio_VERIFY(m_count.value-- > 0);
			return m_multiplexer->bind_io_slot(++m_index.value, storage);
		}

		friend class iocp_multiplexer;
	};

	result<io_slot_list> acquire_io_slot()
	{
		allio_TRY(index, acquire_io_slots_internal(1));
		return { result_value, this, index, 1 };
	}

	result<io_slot*> acquire_io_slot(async_operation_storage& storage)
	{
		allio_TRY(index, acquire_io_slots_internal(1));
		return &bind_io_slot(index, storage);
	}

	result<io_slot_list> acquire_io_slots(size_t const count)
	{
		allio_TRY(index, acquire_io_slots_internal(count));
		return { result_value, this, index, count };
	}


	static bool supports_synchronous_completion(handle const& handle)
	{
		return supports_synchronous_completion(get_flags(handle));
	}

	void post_synchronous_completion(async_operation_storage& storage, uintptr_t information = 0);

	result<void> cancel(native_platform_handle handle, async_operation_storage& storage);

private:
	result<size_t> acquire_io_slots_internal(size_t count);
	void release_io_slots_internal(size_t count);

	io_slot& bind_io_slot(size_t const index, async_operation_storage& storage)
	{
		allio_ASSERT(storage.m_slot == nullptr);
		io_slot& slot = m_slots[m_slot_ring[index & m_slot_count - 1]];
		slot.m_storage = &storage;
		storage.m_slot = &slot;
		return slot;
	}

	void free_io_slot(io_slot& slot);
};

} //namespace allio::win32
