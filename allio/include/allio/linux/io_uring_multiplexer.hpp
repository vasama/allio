#pragma once

#include <allio/deferring_multiplexer.hpp>
#include <allio/detail/api.hpp>
#include <allio/detail/capture.hpp>
#include <allio/detail/concepts.hpp>
#include <allio/linux/detail/unique_fd.hpp>
#include <allio/multiplexer.hpp>
#include <allio/platform_handle.hpp>

#include <vsm/assert.h>

#include <atomic>
#include <bit>
#include <memory>
#include <mutex>
#include <optional>

#include <allio/linux/detail/undef.i>

struct io_uring_sqe;

namespace allio::linux {

class io_uring_multiplexer final : public deferring_multiplexer
{
	// Left incomplete to prevent arithmetic.
	struct sqe_placeholder;
	struct cqe_placeholder;

	struct sqe_type
	{
		static constexpr uint8_t flag_fixed_file                        = 1 << 0;
		static constexpr uint8_t flag_io_drain                          = 1 << 1;
		static constexpr uint8_t flag_io_link                           = 1 << 2;
		static constexpr uint8_t flag_io_hardlink                       = 1 << 3;
		static constexpr uint8_t flag_async                             = 1 << 4;
		static constexpr uint8_t flag_buffer_select                     = 1 << 5;
		static constexpr uint8_t flag_cqe_skip_success                  = 1 << 6;

		uint8_t opcode;
		uint8_t flags;
		uint16_t ioprio;
		int32_t fd;
		union
		{
			uint64_t off;
			uint64_t addr2;
			struct
			{
				uint32_t cmd_op;
				uint32_t _pad1;
			};
		};
		union
		{
			uint64_t addr;
			uint64_t splice_off_in;
		};
		uint32_t len;
		uint32_t _op_flags;
		uint64_t user_data;
		union
		{
			uint16_t buf_index;
			uint16_t buf_group;
		};
		uint16_t personality;
		union
		{
			uint32_t splice_fd_in;
			int32_t file_index;
		};
		union
		{
			struct
			{
				uint64_t addr3;
				uint64_t _pad2;
			};
		};
	};
	static_assert(sizeof(sqe_type) == 64);

	struct cqe_type
	{
		uint64_t user_data;
		int32_t res;
		uint32_t flags;
	};
	static_assert(sizeof(cqe_type) == 16);

	struct flags
	{
		static constexpr uint32_t submit_lock                           = 1 << 0;
		static constexpr uint32_t enter_ext_arg                         = 1 << 1;
		static constexpr uint32_t kernel_thread                         = 1 << 2;
	};

public:
	class async_operation_storage : public deferring_multiplexer::async_operation_storage
	{
		async_operation_storage* m_next;
		async_operation_storage* m_prev;

	protected:
		virtual void io_completed(io_uring_multiplexer& multiplexer, io_slot& slot, int result) = 0;

	private:
		friend class io_uring_multiplexer;
	};

private:
	class mmapping_deleter
	{
		static constexpr size_t borrow_value = static_cast<size_t>(-1);

		size_t m_size;

	public:
		mmapping_deleter(size_t const size)
			: size(size)
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
				release(const_cast<void*>(addr), size);
			}
		}

	private:
		static void release(void* addr, size_t size);
	};

	using unique_mmapping = std::unique_ptr<std::byte, mmapping_deleter>;


	detail::unique_fd const m_io_uring;

	unique_mmapping const m_sq_mmap;
	unique_mmapping const m_cq_mmap;

	uint32_t const m_flags;

	// Sizes of SQE and CQE.
	uint8_t const m_sqe_size;
	uint8_t const m_cqe_size;

	// Shifts used for multiplication by the sizes of SQE and CQE.
	uint8_t const m_sqe_multiply_shift;
	uint8_t const m_cqe_multiply_shift;

	// One past the newest submission queue entry produced by user space.
	// Written by user space, read by the kernel.
	std::atomic_ref<uint32_t> const m_sq_k_produce;

	// One past the newest submission queue entry consumed by the kernel.
	// The value always trails m_sq_k_produce.
	// Written by the kernel, read by user space.
	std::atomic_ref<uint32_t> const m_sq_k_consume;

	// One past the newest completion queue entry produced by the kernel.
	// Written by the kernel, read by user space.
	std::atomic_ref<uint32_t> const m_cq_k_produce;

	// One past the newest completion queue entry consumed by user space.
	// The value always trails m_cq_k_produce.
	// Written by user space, read by the kernel.
	std::atomic_ref<uint32_t> const m_cq_k_consume;

	// Describes the io uring dynamic state.
	// Written by the kernel, read by user space.
	std::atomic_ref<uint32_t> const m_sq_k_flags;

	// Maps logical SQE index to physical index in m_sqes.
	// Written by user space, read by user space and the kernel.
	uint32_t* const m_sq_k_array;

	// Ring of submission queue entries.
	// Written by user space, read by the kernel.
	sqe_placeholder* const m_sqes;

	// Ring of completion queue entries.
	// Written by the kernel, read by user space.
	cqe_placeholder* const m_cqes;

	uint32_t const m_sq_size;
	uint32_t const m_cq_size;

	// 
	uint32_t m_sq_produce_pending;

	uint32_t m_sq_available;

	// Number of free CQEs available.
	uint32_t m_cq_available;

	// The value always trails m_sq_k_consume.
	uint32_t m_sq_acquire;

	// The value always trails m_sq_acquire.
	uint32_t m_sq_release;

	// The value always trails m_sq_release.
	uint32_t m_sq_submit;


	class sqe_index_deleter
	{
		io_uring_multiplexer* m_multiplexer;

	public:
		sqe_index_deleter(io_uring_multiplexer& multiplexer)
			: m_multiplexer(&multiplexer)
		{
		}

		void operator()(uint32_t const sqe_index) const
		{
			m_multiplexer->release_temporary_sqe(sqe_index);
		}
	};
	using unique_sqe_index = vsm::unique_resource<uint32_t, sqe_index_deleter>;

public:
	struct init_options
	{
		uint32_t min_submission_queue_size = 0;
		uint32_t min_completion_queue_size = 0;
		bool enable_kernel_polling_thread = false;
		io_uring_multiplexer const* share_kernel_polling_thread = nullptr;
	};

	class init_result
	{
		detail::unique_fd io_uring;
		unique_mmapping sq_ring;
		unique_mmapping cq_ring;
		unique_mmapping sqes;

		struct
		{
			uint32_t flags;
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


	class submit_context
	{
		io_uring_multiplexer* m_multiplexer;
		async_operation_storage* m_operation;
		uint32_t m_sq_produce_pending;

	public:
		vsm::result<void> push(auto&& initialize)
		{
			vsm_try(sqe, acquire_sqe());

			static_cast<decltype(initialize)&&>(initialize)(*sqe);
			auto const sqe_view = reinterpret_cast<sqe_type const*>(sqe);

			// Users are not allowed to set the user_data field.
			vsm_assert(sqe_view->user_data == 0);

			//TODO: Check which flags are safe for users.
			static constexpr uint8_t user_flags = 0
				| sqe_type::flag_io_link
				| sqe_type::flag_io_hardlink
			;

			// Users are not allowed to set certain sensitive flags.
			vsm_assert((sqe_view->flags & ~user_flags) != 0);

			return {};
		}

		vsm::result<void> push_linked_timeout(deadline const deadline);

	private:
		explicit submit_context(io_uring_multiplexer& multiplexer, async_operation_storage& operation)
			: m_multiplexer(&multiplexer)
			, m_operation(&operation)
			, m_sq_produce_pending(multiplexer.m_sq_produce_pending)
		{
			vsm_assert((std::exchange(m_multiplexer->m_flags, m_multiplexer->m_flags | submit_lock) & submit_lock) == 0);
		}

		submit_context(submit_context const&) = delete;
		submit_context& operator=(submit_context const&) = delete;

		~submit_context()
		{
			vsm_assert((std::exchange(m_multiplexer->m_flags, m_multiplexer->m_flags & ~submit_lock) & submit_lock) != 0);
		}


		vsm::result<io_uring_sqe*> acquire_sqe();

		friend class io_uring_multiplexer;
	};

	vsm::result<void> submit(async_operation_storage& operation, auto&& submit)
	{
		submit_context context(*this, operation);

		vsm::result<void> r = static_cast<decltype(submit)&&>(submit)(context);

		if (r)
		{
			submit_finalize(context);
		}

		return r;
	}

	vsm::result<void> push_linked_timeout();


private:
	io_uring_sqe* get_sqe(uint32_t const index)
	{
		vsm_assert(index < m_sq_size); // Mask first.
		return reinterpret_cast<io_uring_sqe*>(
			reinterpret_cast<std::byte*>(m_sqes) + (index << m_sqe_multiply_shift));
	}

	io_uring_cqe* get_sqe(uint32_t const index)
	{
		vsm_assert(index < m_cq_size); // Mask first.
		return reinterpret_cast<io_uring_cqe*>(
			reinterpret_cast<std::byte*>(m_cqes) + (index << m_cqe_multiply_shift));
	}


	vsm::result<void> submit_finalize();

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
