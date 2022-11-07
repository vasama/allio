#include <allio/linux/io_uring_multiplexer.hpp>

#include <allio/detail/assert.hpp>
#include <allio/implementation.hpp>

#include <allio/impl/async_handle_types.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/timeout.hpp>

#include <bit>

#include <cstring>

#include <linux/io_uring.h>
#include <linux/time64.h>
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


namespace impl_flags {

static constexpr uint32_t enter_ext_arg                         = 1 << 16;
static constexpr uint32_t kernel_thread                         = 1 << 17;
static constexpr uint32_t sqe_extended                          = 1 << 18;
static constexpr uint32_t cqe_extended                          = 1 << 19;

} // namespace impl_flags
} // namespace

static bool wrap_lt(uint32_t const lhs, uint32_t const rhs)
{
	return rhs - lhs < 0x80000000;
}


static bool has_kernel_thread(uint32_t const flags)
{
	return (flags & impl_flags::kernel_thread) != 0;
}


void io_uring_multiplexer::mmapping_deleter::release(void* const addr, size_t const size)
{
	allio_VERIFY(munmap(addr, size) != -1);
}

result<io_uring_multiplexer::init_result> io_uring_multiplexer::init(init_options const& options)
{
	auto const round_up_to_po2 = [](uint32_t const value) -> result<uint32_t>
	{
		uint32_t const lz = std::countl_zero(value);
		if (lz == 0 && (value & value - 1) != 0)
		{
			return allio_ERROR(error::invalid_argument);
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
			params.wq_fd = options.share_kernel_polling_thread->m_io_uring.get();
		}
	}

	allio_TRY(io_uring, [&]() -> result<detail::unique_fd>
	{
		int const io_uring = io_uring_setup(min_entries, &params);

		if (io_uring == -1)
		{
			return allio_ERROR(get_last_error());
		}

		return result<detail::unique_fd>(result_value, io_uring);
	}());


	auto const mmap = [&](size_t const size, uint64_t const offset) -> result<unique_mmapping>
	{
		void* const addr = ::mmap(
			nullptr,
			size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_POPULATE,
			io_uring.get(),
			offset);

		if (addr == MAP_FAILED)
		{
			return allio_ERROR(get_last_error());
		}

		return result<unique_mmapping>(result_value, reinterpret_cast<std::byte*>(addr), size);
	};

	allio_TRY(rings, [&]() -> result<std::pair<unique_mmapping, unique_mmapping>>
	{
		using result_type = result<std::pair<unique_mmapping, unique_mmapping>>;

		size_t sq_size = params.sq_off.array + params.sq_entries * sizeof(uint32_t);
		size_t cq_size = params.cq_off.cqes + params.cq_entries * cqe_size(params.flags);

		if ((params.features & IORING_FEAT_SINGLE_MMAP) != 0)
		{
			sq_size = cq_size = std::max(sq_size, cq_size);
		}

		allio_TRY(sq_ring, mmap(sq_size, IORING_OFF_SQ_RING));

		allio_TRY(cq_ring, [&]() -> result<unique_mmapping>
		{
			if ((params.features & IORING_FEAT_SINGLE_MMAP) != 0)
			{
				return result_type(result_value, sq_ring.get(), mmapping_deleter::borrow());
			}

			return mmap(cq_size, IORING_OFF_CQ_RING);
		}());

		return result_type(result_value, std::move(sq_ring), std::move(cq_ring));
	}());

	auto&& [sq_ring, cq_ring] = std::move(rings);

	allio_TRY(sqes, mmap(params.sq_entries * sqe_size(params.flags), IORING_OFF_SQES));


	result<init_result> r(result_value);
	r->io_uring = std::move(io_uring);
	r->sq_ring = std::move(sq_ring);
	r->cq_ring = std::move(cq_ring);
	r->sqes = std::move(sqes);
	r->params =
	{
		.sqe_multiply_shift = std::countr_zero(64),
		.cqe_multiply_shift = std::countr_zero(32),
		//TODO: Should these read from the ring_entries offsets instead?
		.sq_entries = params.sq_entries,
		.cq_entries = params.cq_entries,
		.sq_off =
		{
			.head = params.sq_off.head,
			.tail = params.sq_off.tail,
			.flags = params.sq_off.flags,
			.array = params.sq_off.array,
		},
		.cq_off =
		{
			.head = params.cq_off.head,
			.tail = params.cq_off.tail,
			.cqes = params.cq_off.cqes,
		},
	};

	if (params.features & IORING_FEAT_EXT_ARG)
	{
		r->params.flags |= flags::enter_ext_arg;
	}
	if (params.flags & IORING_SETUP_SQPOLL)
	{
		r->params.flags |= flags::kernel_thread;
	}
	if (params.flags & IORING_SETUP_SQE128)
	{
		r->params.sqe_multiply_shift = std::countr_zero(128);
	}
	if (params.flags & IORING_SETUP_CQE32)
	{
		r->params.cqe_multiply_shift = std::countr_zero(32);
	}

	return r;
}

io_uring_multiplexer::io_uring_multiplexer(init_result&& resources)
	: m_io_uring((
		allio_ASSERT(resources.io_uring),
		std::move(resources.io_uring)))
	, m_sq_mmap(std::move(resources.sq_ring))
	, m_cq_mmap(std::move(resources.cq_ring))
	, m_flags(resources.params.flags)
	, m_sq_k_produce(*reinterpret_cast<uint32_t*>(m_sq_mmap.get() + resources.params.sq_off.tail))
	, m_sq_k_consume(*reinterpret_cast<uint32_t*>(m_sq_mmap.get() + resources.params.sq_off.head))
	, m_sq_k_flags(*reinterpret_cast<uint32_t*>(m_sq_mmap.get() + resources.params.sq_off.flags))
	, m_sq_k_array(reinterpret_cast<uint32_t*>(m_sq_mmap.get() + resources.params.sq_off.array))
	, m_cq_k_consume(*reinterpret_cast<uint32_t*>(m_cq_mmap.get() + resources.params.cq_off.head))
	, m_cq_k_produce(*reinterpret_cast<uint32_t*>(m_cq_mmap.get() + resources.params.cq_off.tail))
	, m_sqes(resources.sqes.release())
	, m_cqes(reinterpret_cast<io_uring_cqe*>(m_cq_mmap.get() + resources.params.cq_off.cqes))
	, m_sq_size(resources.params.sq_entries)
	, m_cq_size(resources.params.cq_entries)
	, m_sq_cq_available(m_cq_size)
	, m_sq_acquire(m_sq_k_consume.load(std::memory_order_relaxed) - m_sq_size)
	, m_sq_release(m_sq_acquire)
	, m_sq_submit(m_sq_release)
{
	for (size_t i = 0; i < m_sq_size; ++i)
	{
		m_sq_k_array[m_sq_acquire + i & m_sq_size - 1] = i;
	}
}

io_uring_multiplexer::~io_uring_multiplexer()
{
	allio_VERIFY(munmap(m_sqes.release(), m_sq_size * sqe_size(m_flags)) != -1);
}

type_id<multiplexer> io_uring_multiplexer::get_type_id() const
{
	return type_of(*this);
}

result<multiplexer_handle_relation const*> io_uring_multiplexer::find_handle_relation(type_id<handle> const handle_type) const
{
	return static_multiplexer_handle_relation_provider<io_uring_multiplexer, async_handle_types>::find_handle_relation(handle_type);
}

result<void> io_uring_multiplexer::register_native_handle(native_platform_handle const handle)
{
	return {};
}

result<void> io_uring_multiplexer::deregister_native_handle(native_platform_handle const handle)
{
	return {};
}

result<statistics> io_uring_multiplexer::pump(pump_parameters const& args)
{
	return gather_statistics([&](statistics& statistics) -> result<void>
	{
		bool const submission = (args.mode & pump_mode::submit) != pump_mode::none;
		bool const completion = (args.mode & pump_mode::complete) != pump_mode::none;
		bool const flush = (args.mode & pump_mode::flush) != pump_mode::none;

		bool need_enter = false;
		uint32_t enter_submission_count = 0;
		uint32_t enter_completion_count = 0;
		uint32_t enter_flags = 0;

		if (submission)
		{
			uint32_t const new_submission_count = flush_submission_queue();

			if (new_submission_count != 0)
			{
				if (has_kernel_thread(m_flags))
				{
					need_enter = true;
					enter_submission_count = new_submission_count;
				}
				else if ((m_sq_k_flags.load(std::memory_order_acquire) & IORING_SQ_NEED_WAKEUP) != 0)
				{
					if (wrap_lt(m_sq_k_consume.load(std::memory_order_acquire), m_sq_submit))
					{
						need_enter = true;
						enter_flags |= IORING_ENTER_SQ_WAKEUP;
					}
				}
			}
		}

		if (completion)
		{
			uint32_t const new_completion_count = flush_completion_queue();

			if (new_completion_count == 0)
			{
				need_enter = true;
				enter_completion_count = 1;
				enter_flags |= IORING_ENTER_GETEVENTS;
			}
		}

		if (need_enter)
		{
			timespec64 enter_timespec;
			io_uring_getevents_arg enter_arg = {};
			io_uring_getevents_arg* p_enter_arg = nullptr;

			if (deadline != deadline::never())
			{
				if (deadline == deadline::instant())
				{
					enter_completion_count = 0;
				}
				else if ((m_flags & impl_flags::enter_ext_arg) != 0)
				{
					enter_flags |= IORING_ENTER_EXT_ARG;
					enter_timespec = make_timespec<decltype(enter_timespec)>(deadline);
					enter_arg.ts = reinterpret_cast<uintptr_t>(enter_timespec.get());
					p_enter_arg = &enter_arg;
				}
				else
				{
					return allio_ERROR(error::unsupported_operation);
				}
			}

			int const enter_result = io_uring_enter(
				m_io_uring.get(),
				enter_submission_count,
				enter_completion_count,
				enter_flags,
				p_enter_arg,
				sizeof(enter_arg));

			if (enter_result == -1)
			{
				return allio_ERROR(get_last_error());
			}

			if (submission)
			{

			}

			if (completion)
			{
				flush_completion_queue();
			}
		}

		return {};
	});
}

result<void> io_uring_multiplexer::submit_committed_sqes()
{
	if ((m_flags & impl_flags::auto_submit) != 0)
	{
		if (has_kernel_thread(m_flags))
		{

		}
		else
		{
			flush_submission_queue();
		}

		allio_TRYV(pump({
			.mode = pump_mode::submit,
			.deadline = deadline::instant(),
		}));
	}
	return {};
}

result<void> io_uring_multiplexer::commit_user_sqe(uint32_t const sqe_index, io_slot& slot)
{
	io_uring_sqe& sqe = m_sqes[sqe_index];

	allio_ASSERT(sqe.user_data == 0);
	sqe.user_data = reinterpret_cast<uintptr_t>(&handler);

	return submit_committed_sqes();
}

result<void> io_uring_multiplexer::cancel_io(io_slot& slot)
{
	if (auto const sqe_index = try_acquire_sqe())
	{
		submit_sqe_internal(*sqe_index, [&](io_uring_sqe& sqe)
		{
			sqe.opcode = IORING_OP_ASYNC_CANCEL;
			sqe.addr = reinterpret_cast<uintptr_t>(&slot);
			sqe.user_data = reinterpret_cast<uintptr_t>(&slot) | user_data_cancel;
		});
	}
	else
	{
		m_cancel_list.push(slot);
	}
}

result<void> io_uring_multiplexer::submit_finalize()
{
	
}

//TODO: These functions cannot be used directly for temporary SQEs.
result<uint32_t> io_uring_multiplexer::acquire_sqe()
{
	allio_TRYV(acquire_cqe());

	uint32_t const sq_consume = m_sq_k_consume.load(std::memory_order_relaxed);
	uint32_t const sq_acquire = m_sq_acquire;

	if (sq_acquire == sq_consume)
	{
		++m_sq_cq_available; // Release previously acquired CQE.
		return allio_ERROR(error::too_many_concurrent_async_operations);
	}

	m_sq_acquire = sq_acquire + 1;
	uint32_t const sqe_index = m_sq_k_array[sq_acquire & m_sq_size - 1];
	memset(&m_sqes[sqe_index], 0, sqe_size(m_flags));
	return sqe_index;
}

void io_uring_multiplexer::release_sqe(uint32_t const sqe_index)
{
	uint32_t const sq_release = m_sq_release++;
	m_sq_k_array[sq_release & m_sq_size - 1] = sqe_index;

	if (has_kernel_thread(m_flags))
	{
		m_sq_k_produce.store(sq_release, std::memory_order_release);
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

uint32_t io_uring_multiplexer::reap_submission_queue()
{
	uint32_t const sq_mask = m_sq_size - 1;
	size_t const sqe_shift = get_sqe_shift(m_flags);

	uint32_t const sq_produce = m_sq_k_produce.load(std::memory_order_relaxed);
	uint32_t const sq_consume = m_sq_k_consume.load(std::memory_order_acquire);

	if (!has_kernel_thread(m_flags))
	{
		m_sq_k_produce.store(sq_release, std::memory_order_release);
	}

	//TODO: Call submission handlers.

	return sq_release - std::exchange(m_sq_submit, sq_release);
}

uint32_t io_uring_multiplexer::reap_completion_queue()
{
	uint32_t const cq_mask = m_cq_size - 1;
	size_t const cqe_shift = get_cqe_shift(m_flags);

	uint32_t const cq_consume = m_cq_k_consume.load(std::memory_order_relaxed);
	uint32_t const cq_produce = m_cq_k_produce.load(std::memory_order_acquire);

	if (cq_consume != cq_produce)
	{
		for (uint32_t cq_index = cq_consume; cq_index != cq_produce; ++cq_index)
		{
			io_uring_cqe const& cqe = m_cqes[(cq_index & cq_mask) << cqe_shift];

			switch (cqe.user_data & user_data_tag_mask)
			{
			case user_data_normal:
				{
					static_assert(user_data_normal == 0);
					io_slot& slot = *reinterpret_cast<io_slot*>(cqe.user_data);

					allio_ASSERT(slot.m_handler != nullptr);

					//TODO: Gather statistics.
					slot.m_handler->io_completed(*this, slot, cqe.res);
				}
				break;

			case user_data_cancel:
				if (cqe.res == 0)
				{
					io_slot& slot = *reinterpret_cast<io_slot*>(cqe.user_data & user_data_ptr_mask);

					allio_ASSERT(slot.m_handler != nullptr);

					//TODO: Gather statistics.
					slot.m_handler->io_completed(*this, slot, -ECANCELED);
				}
				break;
			}
		}

		m_cq_k_consume.store(cq_produce, std::memory_order_release);
	}

	return cq_produce - cq_consume;
}

result<void> io_uring_multiplexer::enter(pump_mode cnost mode, deadline const deadline)
{

}

allio_TYPE_ID(io_uring_multiplexer);