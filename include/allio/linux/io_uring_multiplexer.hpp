#pragma once

#include <allio/detail/api.hpp>
#include <allio/detail/assert.hpp>
#include <allio/detail/capture.hpp>
#include <allio/detail/concepts.hpp>
#include <allio/linux/detail/unique_fd.hpp>
#include <allio/multiplexer.hpp>
#include <allio/platform_handle.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>

#include <linux/io_uring.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

class io_uring_multiplexer final : public multiplexer
{
public:
	class alignas(uintptr_t) async_operation_storage : public async_operation
	{
		async_operation_storage* m_next_submitted;
		async_operation_storage* m_next_completed;

		result<void>(*m_capture_result)(async_operation_storage& storage, int result) = nullptr;

	public:
		async_operation_storage(async_operation_parameters const&, async_operation_listener* const listener)
			: async_operation(listener)
		{
		}

		template<typename Callable>
		void capture_result(Callable const callable)
		{
			allio_ASSERT(m_capture_result == nullptr);
			m_capture_result = detail::capture_traits<async_operation_storage, int, decltype(&decltype(callable)::operator())>::callback;
		}

	private:
		result<void> set_result(int const result)
		{
			if (m_capture_result != nullptr)
			{
				return m_capture_result(*this, result);
			}
			return {};
		}

		friend class io_uring_multiplexer;
	};

	template<typename Operation>
	struct basic_async_operation_storage
		: async_operation_storage
		, io::parameters_with_result<Operation>
	{
		basic_async_operation_storage(io::parameters_with_result<Operation> const& arguments, async_operation_listener* const listener)
			: async_operation_storage(arguments, listener)
			, io::parameters_with_result<Operation>(arguments)
		{
		}
	};

private:
	struct mmapping_deleter
	{
		static constexpr size_t dont_unmap = static_cast<size_t>(-1);

		size_t size;

		mmapping_deleter() = default;

		mmapping_deleter(size_t const size)
			: size(size)
		{
		}

		static void release(void* addr, size_t size);

		void operator()(void const* const addr) const
		{
			release(const_cast<void*>(addr), size);
		}
	};
	using unique_mmapping = std::unique_ptr<std::byte, mmapping_deleter>;

	template<auto Next>
	struct defer_list
	{
		async_operation_storage* m_head = nullptr;
		async_operation_storage* m_tail = nullptr;

		defer_list() = default;
		defer_list(defer_list const&) = delete;
		defer_list& operator=(defer_list const&) = delete;

		void defer(async_operation_storage* storage);
		void defer(defer_list& list);
		void slice(defer_list& list, async_operation_storage* tail);
		void flush(auto&& fn);
	};

	struct defer_context;

	int m_io_uring;

	std::byte* m_sq_mmap_addr;
	size_t m_sq_mmap_size;

	std::byte* m_cq_mmap_addr;
	size_t m_cq_mmap_size;

	uint32_t m_flags;
	uint32_t m_features;

	uint32_t* m_sq_k_produce; // Mutated by allio.
	uint32_t* m_sq_k_consume; // Trails *m_sq_k_produce.
	uint32_t* m_sq_k_flags;
	uint32_t* m_sq_k_array;

	uint32_t* m_cq_k_produce; // Mutated by the kernel.
	uint32_t* m_cq_k_consume; // Trails *m_cq_k_produce.

	io_uring_sqe* m_sqes;
	io_uring_cqe* m_cqes;

	uint32_t m_sq_size;
	uint32_t m_cq_size;

	size_t m_synchronous_completion_count;
	defer_list<&async_operation_storage::m_next_completed> m_synchronous_completion_list;

	struct alignas(64) // Exclusive access by the submission thread.
	{
		std::optional<std::mutex> m_sq_mutex;

		uint32_t m_sq_cq_available; // Number of free CQEs owned by the submission thread.

		uint32_t m_sq_acquire; // Trails *m_sq_k_consume.
		uint32_t m_sq_release; // Trails m_sq_acquire.
		uint32_t m_sq_submit; // Trails m_sq_release.

		defer_list<&async_operation_storage::m_next_submitted> m_submitted_list;
	};

	struct alignas(64) // Exclusive access by the completion thread.
	{
		std::optional<std::mutex> m_cq_mutex;
	};

	struct alignas(64) // Shared access by both submission and completion threads.
	{
		std::atomic<uint32_t> m_cq_cq_available; // Number of free CQEs produced by the completion thread.
	};

public:
	struct init_options
	{
		uint32_t min_submission_queue_size = 0;
		uint32_t min_completion_queue_size = 0;
		bool enable_concurrent_submission = false;
		bool enable_concurrent_completion = false;
		bool enable_kernel_polling_thread = false;
		io_uring_multiplexer const* share_kernel_polling_thread = nullptr;
	};

	class init_result
	{
		io_uring_params params;
		detail::unique_fd io_uring;
		unique_mmapping sq_ring;
		unique_mmapping cq_ring;
		unique_mmapping sqes;

		friend class io_uring_multiplexer;
	};

	static result<init_result> init(init_options const& options);


	io_uring_multiplexer(init_result&& resources);
	io_uring_multiplexer(io_uring_multiplexer const&) = delete;
	io_uring_multiplexer& operator=(io_uring_multiplexer const&) = delete;
	~io_uring_multiplexer() override;


	type_id<multiplexer> get_type_id() const override;

	result<multiplexer_handle_relation const*> find_handle_relation(type_id<handle> handle_type) const override;


	result<void> submit(deadline deadline) override;
	result<void> poll(deadline deadline) override;
	result<void> submit_and_poll(deadline deadline) override;


	result<void*> register_native_handle(native_platform_handle handle);
	result<void> deregister_native_handle(native_platform_handle handle);


	template<std::derived_from<async_operation_storage> Storage = async_operation_storage>
	using init_sqe_callback = void(Storage& storage, io_uring_sqe& sqe);

	result<void> push(async_operation_storage& storage, init_sqe_callback<>* init_sqe);

	template<std::derived_from<async_operation_storage> Storage>
	result<void> push(Storage& storage, init_sqe_callback<Storage>* const init_sqe)
	{
		return push(static_cast<async_operation_storage&>(storage), reinterpret_cast<init_sqe_callback<>*>(init_sqe));
	}

	void post_synchronous_completion(async_operation_storage& storage, int result = 0);

	result<void> cancel(async_operation_storage& storage);

private:
	static std::unique_lock<std::mutex> lock(std::optional<std::mutex>& mutex);

	io_uring_sqe& use_sqe(uint32_t sqe_index);

	result<uint32_t> acquire_sqe();
	void release_sqe(uint32_t sqe_index);

	result<void> acquire_cqe();
	void release_cqe();

	result<void> push_internal(async_operation_storage& storage, auto&& init_sqe);

	uint32_t flush_submission_queue(defer_context& defer_context);
	uint32_t flush_completion_queue(defer_context& defer_context);

	result<void> enter(defer_context& defer_context, bool submit, bool complete, deadline deadline);
};

} // namespace allio::linux

allio_API extern allio_TYPE_ID(allio::linux::io_uring_multiplexer);

#include <allio/linux/detail/undef.i>
