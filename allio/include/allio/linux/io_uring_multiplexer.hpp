#pragma once

#include <allio/deferring_multiplexer.hpp>
#include <allio/detail/api.hpp>
#include <allio/linux/detail/unique_fd.hpp>
#include <allio/multiplexer.hpp>
#include <allio/platform_handle.hpp>

#include <vsm/assert.h>
#include <vsm/concepts.hpp>
#include <vsm/flags.hpp>
#include <vsm/intrusive/forward_list.hpp>
#include <vsm/utility.hpp>

#include <atomic>
#include <bit>
#include <memory>
#include <mutex>
#include <optional>

#include <allio/linux/detail/undef.i>

struct io_uring_sqe;
struct io_uring_cqe;

namespace allio::linux {

class io_uring_multiplexer final : public deferring_multiplexer
{
	/// Submission ring buffer.
	///
	/// Each index is a position in the submission ring buffer.
	///
	/// The indicies marked with `k` are shared with the kernel.
	///
	/// Each index is limited to less than or equal to the next.
	/// Wraparound is possible, but that does not matter.
	/// Imagine instead that the indices have infinite precision.
	///
	/// @verbatim
	/// |
	/// | <- @ref m_sq_k_consume
	/// |    One after the entry most recently read by the kernel.
	/// |    That slot can now be reused for another entry.
	/// |
	/// | <- @ref m_sq_k_produce
	/// |    One after the entry most recently submitted to the kernel.
	/// |
	/// | <- @ref m_sq_release
	/// |    One after the entry most recently recorded, and which is now ready for submission.
	/// |
	/// | <- @ref m_sq_acquire
	/// |    Free slot where a new entry can be recorded.
	/// |
	/// | <- @ref m_sq_k_consume
	/// |    The ring buffer wraps around.
	/// V
	/// @endverbatim

	/// Completion queue ring buffer
	///
	/// @verbatim
	/// |
	/// V
	/// @endverbatim

	// Intentionally incomplete to prevent arithmetic.
	struct sqe_placeholder;
	struct cqe_placeholder;

	enum class flags : uint32_t
	{
		submit_lock                     = 1 << 0,
		enter_ext_arg                   = 1 << 1,
		kernel_thread                   = 1 << 2,
		auto_submit                     = 1 << 3,
		submit_handler                  = 1 << 4,
	};
	vsm_flag_enum_friend(flags);

	struct io_slot_link : vsm::intrusive::forward_list_link {};

public:
	class alignas(4) io_data
	{
	protected:
		io_data() = default;
		io_data(io_data const&) = default;
		io_data& operator=(io_data const&) = default;
		~io_data() = default;
	};

	class io_data_ref;

	class async_operation_storage
		: public io_data
		, public deferring_multiplexer::async_operation_storage
	{
	public:
		using deferring_multiplexer::async_operation_storage::async_operation_storage;

	protected:
		virtual void io_completed(io_uring_multiplexer& multiplexer, io_data_ref data, int result);

	private:
		friend class io_uring_multiplexer;
	};

	class io_slot : public io_data, io_slot_link
	{
		async_operation_storage* m_operation = nullptr;

	public:
		void set_handler(async_operation_storage& operation)
		{
			m_operation = &operation;
		}

	private:
		friend class io_uring_multiplexer;
	};

private:
	using io_data_ptr = vsm::tag_ptr<io_data, uintptr_t, 1>;

public:
	class io_data_ref : io_data_ptr
	{
	public:
		io_data_ref(async_operation_storage& storage)
			: io_data_ptr(&storage, 0)
		{
		}

		io_data_ref(io_slot& slot)
			: io_data_ptr(&slot, 1)
		{
		}

	private:
		friend class io_uring_multiplexer;
	};

private:
	class mmapping_deleter
	{
		static constexpr size_t borrow_value = static_cast<size_t>(-1);

		size_t m_size;

	public:
		mmapping_deleter() = default;

		explicit mmapping_deleter(size_t const size)
			: m_size(size)
		{
		}

		static mmapping_deleter borrow()
		{
			return mmapping_deleter(borrow_value);
		}

		void operator()(void const* const addr) const
		{
			if (m_size != borrow_value)
			{
				release(const_cast<void*>(addr), m_size);
			}
		}

	private:
		static void release(void* addr, size_t size);
	};

	template<typename T>
	using unique_mmapping = std::unique_ptr<T, mmapping_deleter>;

	template<typename T = std::byte>
	static vsm::result<unique_mmapping<T>> mmap(int io_uring, uint64_t offset, size_t size);


	/// @brief Unique owner of the io uring kernel object.
	unique_fd const m_io_uring;

	/// @brief Unique owner of the mmapped region containing the submission queue indices.
	unique_mmapping<std::byte> const m_sq_mmap;

	/// @brief Unique owner of the mmapped region containing the completion queue entries.
	/// @note Null in the case that the kernel supports a single mmap for both queues.
	unique_mmapping<std::byte> const m_cq_mmap;

	/// @brief Ring of submission queue entries.
	/// @note Written by user space, read by the kernel.
	unique_mmapping<sqe_placeholder> const m_sqes;

	/// @brief Ring of completion queue entries.
	/// @note Written by the kernel, read by user space.
	cqe_placeholder* const m_cqes;

	flags const m_flags;

	/// @brief Dynamic size of a single @ref io_uring_sqe.
	uint8_t const m_sqe_size;

	/// @brief Dynamic size of a single @ref io_uring_cqe.
	uint8_t const m_cqe_size;

	/// @brief Shift used for multiplication by @ref m_sqe_size.
	uint8_t const m_sqe_multiply_shift;

	/// @brief Shift used for multiplication by @ref m_cqe_size.
	uint8_t const m_cqe_multiply_shift;

	/// @brief One past the newest submission queue entry produced by user space.
	/// @note Written by user space, read by the kernel.
	std::atomic_ref<uint32_t> const m_sq_k_produce;

	/// @brief One past the newest submission queue entry consumed by the kernel.
	/// @note The value always trails @ref m_sq_k_produce.
	/// @note Written by the kernel, read by user space.
	std::atomic_ref<uint32_t> const m_sq_k_consume;

	/// @brief One past the newest completion queue entry produced by the kernel.
	/// @note Written by the kernel, read by user space.
	std::atomic_ref<uint32_t> const m_cq_k_produce;

	/// @brief One past the newest completion queue entry consumed by user space.
	/// @note The value always trails @ref m_cq_k_produce.
	/// @note Written by user space, read by the kernel.
	std::atomic_ref<uint32_t> const m_cq_k_consume;

	/// @brief Describes the io uring dynamic state.
	/// @note Written by the kernel, read by user space.
	std::atomic_ref<uint32_t> const m_sq_k_flags;

	/// @brief Maps logical SQE index to physical index in @ref m_sqes.
	/// @note Written by user space, read by user space and the kernel.
	uint32_t* const m_sq_k_array;

	/// @brief Size of the submission queue buffer.
	uint32_t const m_sq_size;

	/// @brief Size of the completion queue buffer.
	uint32_t const m_cq_size;

	/// @brief One past the newest submission queue entry ready
	/// for submission to the kernel via @ref m_sq_k_produce.
	/// @note The value always trails @ref m_sq_k_consume.
	uint32_t m_sq_ready;

	uint32_t m_sq_available;

	// Number of free CQEs available.
	uint32_t m_cq_available;

	/// @note The value always trails @ref m_sq_k_consume.
	uint32_t m_sq_acquire;

	/// @note The value always trails @ref m_sq_acquire.
	uint32_t m_sq_release;

	/// @note The value always trails @ref m_sq_release.
	uint32_t m_sq_submit;

	uint32_t m_cq_consume;

	/// @brief List of cancelled I/Os waiting for free SQEs.
	vsm::intrusive::forward_list<io_slot_link> m_cancel_list;


public:
	static bool is_supported();


	struct init_options
	{
		uint32_t min_submission_queue_size = 0;
		uint32_t min_completion_queue_size = 0;
		bool enable_kernel_polling_thread = false;
		io_uring_multiplexer const* share_kernel_polling_thread = nullptr;
	};

	class init_result
	{
		unique_fd io_uring;
		unique_mmapping<std::byte> sq_ring;
		unique_mmapping<std::byte> cq_ring;
		unique_mmapping<sqe_placeholder> sqes;

		struct
		{
			io_uring_multiplexer::flags flags;
			uint8_t sqe_multiply_shift;
			uint8_t cqe_multiply_shift;
			uint32_t sq_entries;
			uint32_t cq_entries;

			struct
			{
				uint32_t head;
				uint32_t tail;
				uint32_t flags;
				uint32_t array;
			}
			sq_off;

			struct
			{
				uint32_t head;
				uint32_t tail;
				uint32_t cqes;
			}
			cq_off;
		}
		params;

		friend class io_uring_multiplexer;
	};

	static vsm::result<init_result> init(init_options const& options);


	io_uring_multiplexer(init_result&& resources);
	~io_uring_multiplexer() override;


	type_id<multiplexer> get_type_id() const override;

	vsm::result<multiplexer_handle_relation const*> find_handle_relation(type_id<handle> handle_type) const override;


	vsm::result<void> register_native_handle(native_platform_handle handle);
	vsm::result<void> deregister_native_handle(native_platform_handle handle);


	using multiplexer::pump;
	vsm::result<statistics> pump(pump_parameters const& args) override;


	class submission_timeout
	{
		struct timespec
		{
			long long tv_sec;
			long long tv_nsec;
		};

		timespec m_timespec;

	public:
		class reference
		{
			submission_timeout const* m_timeout;
			bool m_absolute;

			friend class submission_timeout;
		};

		reference set_deadline(deadline const deadline) &
		{
			m_timespec = make_timespec(deadline);
			return { this, deadline.is_absolute() };
		}
	};

	class submission_context
	{
		io_uring_multiplexer* m_multiplexer;
		uint32_t m_sq_ready;

	public:
		explicit submission_context(io_uring_multiplexer& multiplexer)
			: m_multiplexer(&multiplexer)
			, m_sq_ready(multiplexer.m_sq_ready)
		{
			vsm_assert(!has_flag(std::exchange(m_multiplexer->m_flags, m_multiplexer->m_flags | flags::submit_lock)));
		}

		submission_context(submission_context const&) = delete;
		submission_context& operator=(submission_context const&) = delete;

		~submission_context()
		{
			vsm_assert(has_flag(std::exchange(m_multiplexer->m_flags, m_multiplexer->m_flags & ~flags::submit_lock)));
		}


		async_result<void> push(io_data_ref const data, std::invocable<io_uring_sqe&> auto&& initialize)
		{
			vsm_try(sqe, acquire_sqe());
			vsm_forward(initialize)(*sqe);

			// Users are not allowed to set the user_data field.
			vsm_assert(check_sqe_user_data(*sqe) &&
				"Use of io_uring_sqe::user_data is not allowed.");

			// Users are not allowed to set certain sensitive flags.
			vsm_assert(check_sqe_flags(*sqe) &&
				"Use of some io_uring_sqe::flags is restricted.");

			//TODO: Set user_data
			//sqe->user_data = reinterpret_pointer_cast<uintptr_t>(data);

			return {};
		}

		async_result<void> push_linked_timeout(submission_timeout::reference timeout);

		async_result<void> commit()
		{
			return m_multiplexer->commit_submission(m_sq_ready);
		}

	private:
		static bool check_sqe_user_data(io_uring_sqe const& sqe) noexcept;
		static bool check_sqe_flags(io_uring_sqe const& sqe) noexcept;

		async_result<io_uring_sqe*> acquire_sqe();

		friend class io_uring_multiplexer;
	};

	async_result<void> record_io(std::invocable<submission_context&> auto&& record)
	{
		submission_context context(*this);
		vsm_try_void(vsm_forward(record)(context));
		return context.commit();
	}

	async_result<void> record_io(io_data_ref const data, std::invocable<io_uring_sqe&> auto&& initialize)
	{
		return record_io([&](submission_context& context) -> async_result<void>
		{
			return context.push(data, vsm_forward(initialize));
		});
	}

	vsm::result<void> cancel_io(io_data_ref data);

private:
	io_uring_sqe& get_sqe(uint32_t const index)
	{
		vsm_assert(index < m_sq_size);
		auto const buffer = reinterpret_cast<std::byte*>(m_sqes.get());
		return *reinterpret_cast<io_uring_sqe*>(buffer + (index << m_sqe_multiply_shift));
	}

	io_uring_cqe& get_cqe(uint32_t const index)
	{
		vsm_assert(index < m_cq_size);
		auto const buffer = reinterpret_cast<std::byte*>(m_cqes);
		return *reinterpret_cast<io_uring_cqe*>(buffer + (index << m_cqe_multiply_shift));
	}

	bool needs_wakeup() const;

	async_result<void> commit_submission(uint32_t sq_ready);

	io_uring_sqe* try_acquire_sqe();

	vsm::result<uint32_t> acquire_sqe();
	void release_sqe(uint32_t sqe_index);

	vsm::result<void> acquire_cqe();
	void release_cqe();

	uint32_t reap_submission_queue();
	uint32_t reap_completion_queue();

	vsm::result<void> enter(pump_mode mode, deadline deadline);
};

} // namespace allio::linux

allio_detail_api extern allio_type_id(allio::linux::io_uring_multiplexer);

#include <allio/linux/detail/undef.i>
