#include <allio/linux/io_uring_multiplexer.hpp>

#include <allio/detail/assert.hpp>
#include <allio/static_multiplexer_handle_relation_provider.hpp>

#include "error.hpp"
#include "../async_handle_types.hpp"

#include <bit>

#include <cstring>

#if __has_include(<linux/time64.h>)
#	include <linux/time64.h>
#else
struct timespec64
{
	uint64_t tv_sec;
	long tv_nsec;
};
#endif

#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

allio_EXTERN_ASYNC_HANDLE_MULTIPLEXER_RELATIONS(io_uring_multiplexer);

namespace {

static int io_uring_setup(unsigned const entries, io_uring_params* const p)
{
	return syscall(__NR_io_uring_setup, entries, p);
}

static int io_uring_enter(int const fd, unsigned const to_submit, unsigned const min_complete, unsigned const flags, void const* const arg, size_t const argsz)
{
	return syscall(__NR_io_uring_enter, fd, to_submit, min_complete, flags, arg, argsz);
}


static constexpr uintptr_t user_data_tag_mask = alignof(io_uring_multiplexer::async_operation_storage) - 1;
static constexpr uintptr_t user_data_ptr_mask = ~user_data_tag_mask;

enum : uintptr_t
{
	user_data_normal,
	user_data_cancel,

	user_data_n
};

static_assert(user_data_n - 1 <= user_data_tag_mask);

} // namespace

static bool wrap_lt(uint32_t const lhs, uint32_t const rhs)
{
	return rhs - lhs < 0x80000000;
}

static size_t get_sqe_shift(uint32_t const flags)
{
	return (flags & IORING_SETUP_SQE128) != 0;
}

static size_t get_sqe_size(uint32_t const flags)
{
	return sizeof(io_uring_sqe) << get_sqe_shift(flags);
}

static size_t get_cqe_shift(uint32_t const flags)
{
	return (flags & IORING_SETUP_CQE32) != 0;
}

static size_t get_cqe_size(uint32_t const flags)
{
	return sizeof(io_uring_cqe) << get_cqe_shift(flags);
}

void io_uring_multiplexer::mmapping_deleter::release(void* const addr, size_t const size)
{
	if (size != dont_unmap)
	{
		allio_VERIFY(munmap(addr, size) != -1);
	}
}

template<auto Next>
void io_uring_multiplexer::defer_list<Next>::defer(async_operation_storage* const storage)
{
	if (m_head == nullptr)
	{
		m_head = storage;
		m_tail = storage;
	}
	else
	{
		m_tail->*Next = storage;
		m_tail = storage;
	}
	storage->*Next = nullptr;
}

template<auto Next>
void io_uring_multiplexer::defer_list<Next>::defer(defer_list& list)
{
	if (list.m_head != nullptr)
	{
		if (m_head == nullptr)
		{
			m_head = std::exchange(list.m_head, nullptr);
		}
		else
		{
			m_tail->*Next = std::exchange(list.m_head, nullptr);
		}
		m_tail = std::exchange(list.m_tail, nullptr);
	}
}

template<auto Next>
void io_uring_multiplexer::defer_list<Next>::flush(auto&& fn)
{
	async_operation_storage* next = m_head;

	while (next != nullptr)
	{
		fn(*std::exchange(next, next->*Next));
	}

	m_head = nullptr;
	m_tail = nullptr;
}

struct io_uring_multiplexer::defer_context
{
	defer_list<&async_operation_storage::m_next_submitted> submitted_list;
	defer_list<&async_operation_storage::m_next_completed> completed_list;

	defer_context() = default;

	defer_context(defer_context const&) = delete;
	defer_context& operator=(defer_context const&) = delete;

	~defer_context()
	{
		submitted_list.flush([](async_operation_storage& storage)
		{
			auto const listener = storage.get_listener();
			allio_ASSERT(listener != nullptr);
			listener->submitted(storage);
		});

		completed_list.flush([](async_operation_storage& storage)
		{
			set_status(storage, async_operation_status::concluded);

			auto const listener = storage.get_listener();
			allio_ASSERT(listener != nullptr);
			listener->completed(storage);
			listener->concluded(storage);
		});
	}
};

result<io_uring_multiplexer::init_result> io_uring_multiplexer::init(init_options const& options)
{
	auto const round_up_to_po2 = [](uint32_t const value) -> result<uint32_t>
	{
		uint32_t const lz = std::countl_zero(value);
		if (lz == 0 && (value & value - 1) != 0)
		{
			return allio_ERROR(make_error_code(std::errc::argument_out_of_domain));
		}
		return static_cast<uint32_t>(1) << (31 - lz);
	};

	allio_TRY(min_entries, round_up_to_po2(std::max(static_cast<uint32_t>(32),
		std::max(options.min_submission_queue_size, options.min_completion_queue_size))));

	io_uring_params params = {};

	if (options.enable_kernel_polling_thread)
	{
		params.flags |= IORING_SETUP_SQPOLL;

		if (options.share_kernel_polling_thread != nullptr)
		{
			params.wq_fd = options.share_kernel_polling_thread->m_io_uring;
		}
	}

	allio_TRY(io_uring, [&]() -> result<detail::unique_fd>
	{
		int const io_uring = io_uring_setup(min_entries, &params);

		if (io_uring == -1)
		{
			return allio_ERROR(get_last_error_code());
		}

		return { result_value, io_uring };
	}());


	auto const mmap_r = [&](size_t const size, uint64_t const offset) -> result<unique_mmapping>
	{
		void* const addr = mmap(
			nullptr,
			size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_POPULATE,
			io_uring.get(),
			offset);

		if (addr == MAP_FAILED)
		{
			return allio_ERROR(get_last_error_code());
		}

		return { result_value, reinterpret_cast<std::byte*>(addr), size };
	};

	allio_TRY(rings, [&]() -> result<std::pair<unique_mmapping, unique_mmapping>>
	{
		size_t sq_size = params.sq_off.array + params.sq_entries * sizeof(uint32_t);
		size_t cq_size = params.cq_off.cqes + params.cq_entries * get_cqe_size(params.flags);

		if ((params.features & IORING_FEAT_SINGLE_MMAP) != 0)
		{
			sq_size = cq_size = std::max(sq_size, cq_size);
		}

		allio_TRY(sq_ring, mmap_r(sq_size, IORING_OFF_SQ_RING));

		allio_TRY(cq_ring, [&]() -> result<unique_mmapping>
		{
			if ((params.features & IORING_FEAT_SINGLE_MMAP) != 0)
			{
				return { result_value, sq_ring.get(), mmapping_deleter::dont_unmap };
			}

			return mmap_r(cq_size, IORING_OFF_CQ_RING);
		}());

		return { result_value, std::move(sq_ring), std::move(cq_ring) };
	}());

	auto&& [sq_ring, cq_ring] = std::move(rings);

	allio_TRY(sqes, mmap_r(params.sq_entries * get_sqe_size(params.flags), IORING_OFF_SQES));


	result<init_result> result = { result_value };
	result->params = params;
	result->io_uring = std::move(io_uring);
	result->sq_ring = std::move(sq_ring);
	result->cq_ring = std::move(cq_ring);
	result->sqes = std::move(sqes);
	return result;
}

io_uring_multiplexer::io_uring_multiplexer(init_result&& resources)
{
	allio_ASSERT(resources.io_uring);

	auto const& params = resources.params;

	m_io_uring = resources.io_uring.release();

	m_sq_mmap_size = resources.sq_ring.get_deleter().size;
	m_sq_mmap_addr = resources.sq_ring.release();

	m_cq_mmap_size = resources.cq_ring.get_deleter().size;
	m_cq_mmap_addr = resources.cq_ring.release();

	m_flags = params.flags;
	m_features = params.features;

	m_sq_k_produce = reinterpret_cast<uint32_t*>(m_sq_mmap_addr + params.sq_off.tail);
	m_sq_k_consume = reinterpret_cast<uint32_t*>(m_sq_mmap_addr + params.sq_off.head);
	m_sq_k_flags = reinterpret_cast<uint32_t*>(m_sq_mmap_addr + params.sq_off.flags);
	m_sq_k_array = reinterpret_cast<uint32_t*>(m_sq_mmap_addr + params.sq_off.array);

	m_cq_k_consume = reinterpret_cast<uint32_t*>(m_cq_mmap_addr + params.cq_off.head);
	m_cq_k_produce = reinterpret_cast<uint32_t*>(m_cq_mmap_addr + params.cq_off.tail);

	m_sqes = reinterpret_cast<io_uring_sqe*>(resources.sqes.release());
	m_cqes = reinterpret_cast<io_uring_cqe*>(m_cq_mmap_addr + params.cq_off.cqes);

	m_sq_size = params.sq_entries;
	m_cq_size = params.cq_entries;

	m_synchronous_completion_count = 0;

	m_sq_cq_available = params.cq_entries;
	m_sq_acquire = *m_sq_k_consume - m_sq_size;
	m_sq_release = m_sq_acquire;
	m_sq_submit = m_sq_acquire;

	for (size_t i = 0; i < m_sq_size; ++i)
	{
		m_sq_k_array[m_sq_acquire + i & m_sq_size - 1] = i;
	}
}

io_uring_multiplexer::~io_uring_multiplexer()
{
	mmapping_deleter::release(m_sqes, m_sq_size * get_sqe_size(m_flags));
	mmapping_deleter::release(m_sq_mmap_addr, m_sq_mmap_size);
	mmapping_deleter::release(m_cq_mmap_addr, m_cq_mmap_size);
	detail::fd_deleter::release(m_io_uring);
}

type_id<multiplexer> io_uring_multiplexer::get_type_id() const
{
	return type_of(*this);
}

result<multiplexer_handle_relation const*> io_uring_multiplexer::find_handle_relation(type_id<handle> const handle_type) const
{
	return static_multiplexer_handle_relation_provider<io_uring_multiplexer, async_handle_types>::find_handle_relation(handle_type);
}

result<void> io_uring_multiplexer::submit(deadline const deadline)
{
	defer_context defer_context;
	auto const sq_lock = lock(m_sq_mutex);
	return enter(defer_context, true, false, deadline);
}

result<void> io_uring_multiplexer::poll(deadline const deadline)
{
	defer_context defer_context;
	auto const cq_lock = lock(m_cq_mutex);
	return enter(defer_context, false, true, deadline);
}

result<void> io_uring_multiplexer::submit_and_poll(deadline const deadline)
{
	defer_context defer_context;
	auto const sq_lock = lock(m_sq_mutex);
	auto const cq_lock = lock(m_cq_mutex);
	return enter(defer_context, true, true, deadline);
}

result<void*> io_uring_multiplexer::register_native_handle(native_platform_handle const handle)
{
	return nullptr;
}

result<void> io_uring_multiplexer::deregister_native_handle(native_platform_handle const handle)
{
	return {};
}

result<void> io_uring_multiplexer::push_internal(async_operation_storage& storage, auto&& init_sqe)
{
	defer_context defer_context;
	auto const sq_lock = lock(m_sq_mutex);

	allio_ASSERT(!storage.is_scheduled());

	allio_TRY(sqe_index, acquire_sqe());
	init_sqe(use_sqe(sqe_index));
	release_sqe(sqe_index);

	set_status(storage, async_operation_status::scheduled);

	if (true) //TODO: enable control of automatic submission somehow
	{
		allio_TRYV(enter(defer_context, true, false, deadline::instant()));
	}
	else if ((m_flags & IORING_SETUP_SQPOLL) != 0)
	{
		flush_submission_queue(defer_context);
	}

	return {};
}

result<void> io_uring_multiplexer::push(async_operation_storage& storage, init_sqe_callback<>* const init_sqe)
{
	return push_internal(storage, [&](io_uring_sqe& sqe)
	{
		init_sqe(storage, sqe);

		allio_ASSERT(sqe.user_data == 0);
		sqe.user_data = reinterpret_cast<uintptr_t>(&storage);

		if (storage.get_listener() != nullptr)
		{
			m_submitted_list.defer(&storage);
		}
	});
}

void io_uring_multiplexer::post_synchronous_completion(async_operation_storage& storage, int const result)
{
	set_result(storage, as_error_code(storage.set_result(result)));
	set_status(storage,
		async_operation_status::completed |
		async_operation_status::concluded);

	if (storage.get_listener() != nullptr)
	{
		++m_synchronous_completion_count;
		m_synchronous_completion_list.defer(&storage);
	}
}

result<void> io_uring_multiplexer::cancel(async_operation_storage& storage)
{
	return push_internal(storage, [&](io_uring_sqe& sqe)
	{
		sqe.opcode = IORING_OP_ASYNC_CANCEL;
		sqe.addr = reinterpret_cast<uintptr_t>(&storage);
		sqe.user_data = reinterpret_cast<uintptr_t>(&storage) | user_data_cancel;
	});
}

std::unique_lock<std::mutex> io_uring_multiplexer::lock(std::optional<std::mutex>& mutex)
{
	if (mutex)
	{
		return std::unique_lock<std::mutex>(*mutex);
	}
	return std::unique_lock<std::mutex>();
}

io_uring_sqe& io_uring_multiplexer::use_sqe(uint32_t const sqe_index)
{
	io_uring_sqe& sqe = m_sqes[sqe_index];
	memset(&sqe, 0, sizeof(sqe) << get_sqe_shift(m_flags));
	return sqe;
}

result<uint32_t> io_uring_multiplexer::acquire_sqe()
{
	allio_TRYV(acquire_cqe());

	auto const sq_k_consume = std::atomic_ref(*m_sq_k_consume);

	uint32_t const sq_consume = sq_k_consume.load(std::memory_order_relaxed);
	uint32_t const sq_acquire = m_sq_acquire;

	if (sq_acquire == sq_consume)
	{
		++m_sq_cq_available; // Release previously acquired CQE.
		return allio_ERROR(error::too_many_concurrent_async_operations);
	}

	m_sq_acquire = sq_acquire + 1;
	return m_sq_k_array[sq_acquire & m_sq_size - 1];
}

void io_uring_multiplexer::release_sqe(uint32_t const sqe_index)
{
	uint32_t const sq_release = m_sq_release++;
	m_sq_k_array[sq_release & m_sq_size - 1] = sqe_index;

	if ((m_flags & IORING_SETUP_SQPOLL) != 0)
	{
		auto const sq_k_produce = std::atomic_ref(*m_sq_k_produce);
		sq_k_produce.store(sq_release, std::memory_order_release);
	}
}

result<void> io_uring_multiplexer::acquire_cqe()
{
	uint32_t sq_cq_available = m_sq_cq_available;

	if (sq_cq_available == 0)
	{
		uint32_t const cq_cq_available = m_cq_cq_available.load(std::memory_order_acquire);

		if (cq_cq_available == 0)
		{
			return allio_ERROR(error::too_many_concurrent_async_operations);
		}

		sq_cq_available = m_cq_cq_available.exchange(0, std::memory_order_acq_rel);
		allio_ASSERT(sq_cq_available >= cq_cq_available);
	}

	m_sq_cq_available = sq_cq_available - 1;

	return {};
}

void io_uring_multiplexer::release_cqe()
{
	(void)m_cq_cq_available.fetch_add(1, std::memory_order_acq_rel);
}

uint32_t io_uring_multiplexer::flush_submission_queue(defer_context& defer_context)
{
	uint32_t const sq_release = m_sq_release;

	if ((m_flags & IORING_SETUP_SQPOLL) == 0)
	{
		auto const sq_k_produce = std::atomic_ref(*m_sq_k_produce);
		sq_k_produce.store(sq_release, std::memory_order_release);
	}

	//TODO: defer submitted

	return sq_release - std::exchange(m_sq_submit, sq_release);
}

uint32_t io_uring_multiplexer::flush_completion_queue(defer_context& defer_context)
{
	size_t const synchronous_completion_count = m_synchronous_completion_count;

	if (synchronous_completion_count > 0)
	{
		m_synchronous_completion_count = 0;
		defer_context.completed_list.defer(m_synchronous_completion_list);
	}


	uint32_t const size = m_cq_size;
	uint32_t const mask = size - 1;

	size_t const shift = get_cqe_shift(m_flags);

	auto const cq_k_consume = std::atomic_ref(*m_cq_k_consume);
	auto const cq_k_produce = std::atomic_ref(*m_cq_k_produce);

	uint32_t const cq_consume = cq_k_consume.load(std::memory_order_relaxed);
	uint32_t const cq_produce = cq_k_produce.load(std::memory_order_acquire);

	if (cq_consume != cq_produce)
	{
		for (uint32_t new_cq_consume = cq_consume; new_cq_consume != cq_produce; ++new_cq_consume)
		{
			io_uring_cqe const& cqe = m_cqes[(new_cq_consume & mask) << shift];
			auto const storage = reinterpret_cast<async_operation_storage*>(cqe.user_data & user_data_ptr_mask);

			switch (cqe.user_data & user_data_tag_mask)
			{
			case user_data_normal:
				{
					set_result(*storage, cqe.res >= 0
						? as_error_code(storage->set_result(cqe.res))
						: std::error_code(-cqe.res, std::system_category()));

					async_operation_status new_status =
						async_operation_status::completed;

					if (storage->get_listener() != nullptr)
					{
						defer_context.completed_list.defer(storage);
					}
					else
					{
						new_status |= async_operation_status::concluded;
					}

					set_status(*storage, new_status);
				}
				break;

			case user_data_cancel:
				if (cqe.res == 0)
				{
					set_result(*storage, error::async_operation_cancelled);

					async_operation_status new_status =
						async_operation_status::completed |
						async_operation_status::cancelled;

					if (storage->get_listener() != nullptr)
					{
						defer_context.completed_list.defer(storage);
					}
					else
					{
						new_status |= async_operation_status::concluded;
					}

					set_status(*storage, new_status);
				}
				break;
			}
		}

		cq_k_consume.store(cq_produce, std::memory_order_release);
	}

	return synchronous_completion_count + (cq_produce - cq_consume);
}

result<void> io_uring_multiplexer::enter(defer_context& defer_context, bool const submission, bool const completion, deadline const deadline)
{
	allio_ASSERT(submission || completion);

	bool need_enter = false;
	uint32_t enter_submission_count = 0;
	uint32_t enter_completion_count = 0;
	uint32_t enter_flags = 0;

	if (submission)
	{
		uint32_t const new_submission_count = flush_submission_queue(defer_context);

		if (new_submission_count != 0)
		{
			if ((m_flags & IORING_SETUP_SQPOLL) == 0)
			{
				need_enter = true;
				enter_submission_count = new_submission_count;
			}
			else if ((std::atomic_ref(*m_sq_k_flags).load(std::memory_order_acquire) & IORING_SQ_NEED_WAKEUP) != 0)
			{
				if (wrap_lt(std::atomic_ref(*m_sq_k_consume).load(std::memory_order_acquire), m_sq_submit))
				{
					need_enter = true;
					enter_flags |= IORING_ENTER_SQ_WAKEUP;
				}
			}
		}
	}

	if (completion)
	{
		uint32_t const new_completion_count = flush_completion_queue(defer_context);

		if (new_completion_count == 0)
		{
			need_enter = true;
			enter_completion_count = 1;
			enter_flags |= IORING_ENTER_GETEVENTS;
		}
	}

	if (need_enter)
	{
		io_uring_getevents_arg enter_arg = {};
		timespec64 enter_timespec;

		if (deadline != deadline::never())
		{
			if (deadline == deadline::instant())
			{
				enter_completion_count = 0;
			}
			else if ((m_features & IORING_FEAT_EXT_ARG) != 0)
			{
				//TODO: finish deadline support
				return allio_ERROR(make_error_code(std::errc::not_supported));

				//TODO: fill enter_timespec

				enter_flags |= IORING_ENTER_EXT_ARG;
				enter_arg.ts = reinterpret_cast<uintptr_t>(&enter_timespec);
			}
			else
			{
				return allio_ERROR(make_error_code(std::errc::not_supported));
			}
		}

		int const enter_result = io_uring_enter(
			m_io_uring,
			enter_submission_count,
			enter_completion_count,
			enter_flags,
			nullptr, 0);
			//&enter_arg,
			//sizeof(enter_arg));
			//TODO: fix arg

		if (enter_result == -1)
		{
			return allio_ERROR(get_last_error_code());
		}

		if (submission)
		{
		}

		if (completion)
		{
			flush_completion_queue(defer_context);
		}
	}

	return {};
}

allio_TYPE_ID(io_uring_multiplexer);