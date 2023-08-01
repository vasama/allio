#include <allio/linux/io_uring_multiplexer.hpp>

#include <allio/implementation.hpp>
#include <allio/impl/async_handle_types.hpp>
#include <allio/impl/linux/error.hpp>

#include <vsm/assert.h>
#include <vsm/tag_ptr.hpp>
#include <vsm/utility.hpp>

#include <bit>

#include <cstring>

#include <linux/io_uring.h>
#include <linux/time_types.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>


#ifndef IORING_SETUP_SQE128
#	define IORING_SETUP_SQE128                  (1U << 10)
#endif

#ifndef IORING_SETUP_CQE32
#	define IORING_SETUP_CQE32                   (1U << 11)
#endif


#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

allio_extern_async_handle_multiplexer_relations(io_uring_multiplexer);

namespace {

static int io_uring_setup(unsigned const entries, io_uring_params* const p)
{
	return syscall(__NR_io_uring_setup, entries, p);
}

static int io_uring_enter(int const fd, unsigned const to_submit, unsigned const min_complete, unsigned const flags, void const* const arg, size_t const argsz)
{
	return syscall(__NR_io_uring_enter, fd, to_submit, min_complete, flags, arg, argsz);
}


#if 0
static constexpr uintptr_t user_data_tag_mask = alignof(io_uring_multiplexer::async_operation_storage) - 1;
static constexpr uintptr_t user_data_ptr_mask = ~user_data_tag_mask;

enum : uintptr_t
{
	user_data_normal,
	user_data_cancel,

	user_data_n
};
static_assert(user_data_n - 1 <= user_data_tag_mask);
#endif


enum user_data_tag : uintptr_t
{
	storage,
	io_slot,
	cancel,
};
using user_data_ptr = vsm::tag_ptr<io_uring_multiplexer::io_data, user_data_tag, user_data_tag::cancel>;

} // namespace

static bool wrap_lt(uint32_t const lhs, uint32_t const rhs)
{
	return rhs - lhs < 0x80000000;
}


void io_uring_multiplexer::mmapping_deleter::release(void* const addr, size_t const size)
{
	vsm_verify(munmap(addr, size) != -1);
}

template<typename T>
vsm::result<io_uring_multiplexer::unique_mmapping<T>> io_uring_multiplexer::mmap(
	int const io_uring, uint64_t const offset, size_t const size)
{
	void* const addr = ::mmap(
		nullptr,
		size,
		PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_POPULATE,
		io_uring,
		offset);

	if (addr == MAP_FAILED)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm::result<unique_mmapping<T>>(
		vsm::result_value,
		reinterpret_cast<T*>(addr),
		mmapping_deleter(size));
};


bool io_uring_multiplexer::is_supported()
{
	static constexpr uint8_t lazy_init_flag = 0x80;
	static constexpr uint8_t supported_flag = 0x01;

	static constinit uint8_t storage = 0;
	auto const reference = std::atomic_ref(storage);

	auto value = reference.load(std::memory_order::acquire);
	if (value == 0)
	{
		errno = 0;
		syscall(__NR_io_uring_register, 0, IORING_UNREGISTER_BUFFERS, NULL, 0);
		value = lazy_init_flag | (errno != ENOSYS ? supported_flag : 0);

		reference.store(value, std::memory_order::release);
	}
	return (value & supported_flag) != 0;
}

vsm::result<io_uring_multiplexer::init_result> io_uring_multiplexer::init(init_options const& options)
{
	auto const round_up_to_po2 = [](uint32_t const value) -> vsm::result<uint32_t>
	{
		uint32_t const lz = std::countl_zero(value);
		if (lz == 0 && (value & value - 1) != 0)
		{
			return vsm::unexpected(error::invalid_argument);
		}
		return static_cast<uint32_t>(1) << (31 - lz);
	};

	vsm_try(min_entries, round_up_to_po2(std::max(static_cast<uint32_t>(32),
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

	uint8_t const sqe_size = params.flags & IORING_SETUP_SQE128 ? 128 : sizeof(io_uring_sqe);
	uint8_t const cqe_size = params.flags & IORING_SETUP_CQE32  ? 32  : sizeof(io_uring_cqe);

	vsm_try(io_uring, [&]() -> vsm::result<unique_fd>
	{
		int const io_uring = io_uring_setup(min_entries, &params);

		if (io_uring == -1)
		{
			return vsm::unexpected(get_last_error());
		}

		return vsm::result<unique_fd>(vsm::result_value, io_uring);
	}());

	using mmapping_pair = std::pair<unique_mmapping<std::byte>, unique_mmapping<std::byte>>;

	vsm_try(rings, [&]() -> vsm::result<mmapping_pair>
	{
		using result_type = vsm::result<mmapping_pair>;

		size_t sq_size = params.sq_off.array + params.sq_entries * sizeof(uint32_t);
		size_t cq_size = params.cq_off.cqes + params.cq_entries * cqe_size;

		if ((params.features & IORING_FEAT_SINGLE_MMAP) != 0)
		{
			sq_size = cq_size = std::max(sq_size, cq_size);
		}

		vsm_try(sq_ring, mmap(io_uring.get(), IORING_OFF_SQ_RING, sq_size));

		vsm_try(cq_ring, [&]() -> vsm::result<unique_mmapping<std::byte>>
		{
			if ((params.features & IORING_FEAT_SINGLE_MMAP) != 0)
			{
				return vsm::result<unique_mmapping<std::byte>>(
					vsm::result_value,
					sq_ring.get(),
					mmapping_deleter::borrow());
			}

			return mmap(io_uring.get(), IORING_OFF_CQ_RING, cq_size);
		}());

		return result_type(vsm::result_value, vsm_move(sq_ring), vsm_move(cq_ring));
	}());

	auto&& [sq_ring, cq_ring] = vsm_move(rings);

	vsm_try(sqes, mmap<sqe_placeholder>(io_uring.get(), IORING_OFF_SQES, params.sq_entries * sqe_size));


	vsm::result<init_result> r(vsm::result_value);
	r->io_uring = vsm_move(io_uring);
	r->sq_ring = vsm_move(sq_ring);
	r->cq_ring = vsm_move(cq_ring);
	r->sqes = vsm_move(sqes);
	r->params =
	{
		.sqe_multiply_shift = static_cast<uint8_t>(std::countr_zero(sqe_size)),
		.cqe_multiply_shift = static_cast<uint8_t>(std::countr_zero(cqe_size)),
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

	return r;
}

io_uring_multiplexer::io_uring_multiplexer(init_result&& resources)
	: m_io_uring((
		vsm_assert(resources.io_uring &&
			"io_uring_multiplexer::init_result was already consumed."),
		vsm_move(resources.io_uring)))
	, m_sq_mmap(vsm_move(resources.sq_ring))
	, m_cq_mmap(vsm_move(resources.cq_ring))
	, m_flags(resources.params.flags)
	, m_sqe_size(1 << resources.params.sqe_multiply_shift)
	, m_cqe_size(1 << resources.params.cqe_multiply_shift)
	, m_sqe_multiply_shift(resources.params.sqe_multiply_shift)
	, m_cqe_multiply_shift(resources.params.cqe_multiply_shift)
	, m_sq_k_produce(*reinterpret_cast<uint32_t*>(m_sq_mmap.get() + resources.params.sq_off.tail))
	, m_sq_k_consume(*reinterpret_cast<uint32_t*>(m_sq_mmap.get() + resources.params.sq_off.head))
	, m_sq_k_flags(*reinterpret_cast<uint32_t*>(m_sq_mmap.get() + resources.params.sq_off.flags))
	, m_sq_k_array(reinterpret_cast<uint32_t*>(m_sq_mmap.get() + resources.params.sq_off.array))
	, m_cq_k_consume(*reinterpret_cast<uint32_t*>(m_cq_mmap.get() + resources.params.cq_off.head))
	, m_cq_k_produce(*reinterpret_cast<uint32_t*>(m_cq_mmap.get() + resources.params.cq_off.tail))
	, m_sqes(vsm_move(resources.sqes))
	, m_cqes(reinterpret_cast<cqe_placeholder*>(m_cq_mmap.get() + resources.params.cq_off.cqes))
	, m_sq_size(resources.params.sq_entries)
	, m_cq_size(resources.params.cq_entries)
	, m_sq_acquire(m_sq_k_consume.load(std::memory_order_relaxed) - m_sq_size)
	, m_sq_submit(m_sq_release)
{
	for (size_t i = 0; i < m_sq_size; ++i)
	{
		m_sq_k_array[m_sq_acquire + i & m_sq_size - 1] = i;
	}
}

io_uring_multiplexer::~io_uring_multiplexer() = default;

type_id<multiplexer> io_uring_multiplexer::get_type_id() const
{
	return type_of(*this);
}

vsm::result<multiplexer_handle_relation const*> io_uring_multiplexer::find_handle_relation(type_id<handle> const handle_type) const
{
	return static_multiplexer_handle_relation_provider<io_uring_multiplexer, async_handle_types>::find_handle_relation(handle_type);
}

vsm::result<void> io_uring_multiplexer::register_native_handle(native_platform_handle const handle)
{
	return {};
}

vsm::result<void> io_uring_multiplexer::deregister_native_handle(native_platform_handle const handle)
{
	return {};
}

#if 0
vsm::result<void> io_uring_multiplexer::submit_committed_sqes()
{
	if ((m_flags & impl_flags::auto_submit) != 0)
	{
		if (has_kernel_thread(m_flags))
		{

		}
		else
		{
			reap_submission_queue();
		}

		vsm_try_void(pump(
		{
			.mode = pump_mode::submit,
			.deadline = deadline::instant(),
		}));
	}
	return {};
}

vsm::result<void> io_uring_multiplexer::commit_user_sqe(uint32_t const sqe_index, io_slot& slot)
{
	io_uring_sqe& sqe = get_sqe(sqe_index);

	vsm_assert(sqe.user_data == 0);
	sqe.user_data = reinterpret_pointer_cast<uintptr_t>(user_data_ptr(&slot));

	return submit_committed_sqes();
}
#endif

vsm::result<void> io_uring_multiplexer::cancel_io(io_data_ref const data)
{
	vsm_assert(!vsm::has_flag(m_flags, flags::submission_lock));

	//TODO: Implement io_uring_multiplexer::cancel_io.



	#if 0
	if (auto const sqe_index = try_acquire_sqe())
	{
		submit_sqe_internal(*sqe_index, [&](io_uring_sqe& sqe)
		{
			sqe.opcode = IORING_OP_ASYNC_CANCEL;
			sqe.addr = reinterpret_pointer_cast<uintptr_t>(user_data_ptr(&slot, user_data_tag::normal));
			sqe.user_data = reinterpret_pointer_cast<uintptr_t>(user_data_ptr(&slot, user_data_tag::cancel));
		});
	}
	else
	{
		m_cancel_list.push(&slot);
	}
	#endif

	return {};
}

#if 0
//TODO: These functions cannot be used directly for temporary SQEs.
vsm::result<uint32_t> io_uring_multiplexer::acquire_sqe()
{
	vsm_try_void(acquire_cqe());

	uint32_t const sq_consume = m_sq_k_consume.load(std::memory_order_relaxed);
	uint32_t const sq_acquire = m_sq_acquire;

	if (sq_acquire == sq_consume)
	{
		++m_sq_cq_available; // Release previously acquired CQE.
		return vsm::unexpected(error::too_many_concurrent_async_operations);
	}

	m_sq_acquire = sq_acquire + 1;
	uint32_t const sqe_index = m_sq_k_array[sq_acquire & m_sq_size - 1];
	memset(&get_sqe(sqe_index), 0, m_sqe_size);
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

vsm::result<void> io_uring_multiplexer::acquire_cqe()
{
	uint32_t sq_cq_available = m_sq_cq_available;

	if (sq_cq_available == 0)
	{
		uint32_t const cq_cq_available = m_cq_cq_available.load(std::memory_order_acquire);

		if (cq_cq_available == 0)
		{
			return vsm::unexpected(error::too_many_concurrent_async_operations);
		}

		sq_cq_available = m_cq_cq_available.exchange(0, std::memory_order_acq_rel);
		vsm_assert(sq_cq_available >= cq_cq_available);
	}

	m_sq_cq_available = sq_cq_available - 1;

	return {};
}

void io_uring_multiplexer::release_cqe()
{
	(void)m_cq_cq_available.fetch_add(1, std::memory_order_acq_rel);
}
#endif

bool io_uring_multiplexer::needs_wakeup() const
{
	return (m_sq_k_flags.load(std::memory_order_acquire) & IORING_SQ_NEED_WAKEUP) != 0;
}

async_result<void> io_uring_multiplexer::submission_context::push_linked_timeout(timeout::reference const timeout)
{
	#if __cpp_lib_is_layout_compatible
	static_assert(std::is_layout_compatible_v<timeout::timespec, __kernel_timespec>);
	#else
	static_assert(sizeof(timeout::timespec) == sizeof(__kernel_timespec));
	static_assert(alignof(timeout::timespec) == alignof(__kernel_timespec));
	#endif

	vsm_try(sqe, acquire_sqe());

	sqe->opcode = IORING_OP_LINK_TIMEOUT;
	sqe->flags = IOSQE_IO_LINK;
	sqe->addr = reinterpret_cast<uintptr_t>(&timeout.m_timeout->m_timespec);
	sqe->len = 1;
	sqe->timeout_flags = timeout.m_absolute ? IORING_TIMEOUT_ABS : 0;

	return {};
}

async_result<void> io_uring_multiplexer::commit_submission(uint32_t const sq_release)
{
	//TODO: Assert sq_release <= ?

	m_sq_release = sq_release;

	m_sq_k_produce.store(sq_release, std::memory_order_release);

	if (vsm::any_flags(m_flags, flags::auto_submit))
	{
		if (vsm::any_flags(m_flags, flags::kernel_thread))
		{
			if (needs_wakeup())
			{
				io_uring_enter(m_io_uring.get(), 0, 0, IORING_ENTER_SQ_WAKEUP, nullptr, 0);
			}
		}
		else
		{
			//TODO: enter
		}
	}

	return {};
}

#if 0
uint32_t io_uring_multiplexer::reap_submission_queue()
{
	//uint32_t const sq_produce = m_sq_produce;

	// One after the last submission queue entry consumed by the kernel.
	uint32_t const sq_consume = m_sq_k_consume.load(std::memory_order_acquire);

	if (vsm::any_flags(m_flags, flags::submit_handler))
	{
		//TODO: Post submitted operations.
	}

	m_sq_acquire = sq_consume;

	//return sq_release - std::exchange(m_sq_submit, sq_release);
}
#endif

uint32_t io_uring_multiplexer::reap_completion_queue()
{
	uint32_t const cq_consume = m_cq_consume;

	// One after the last completion queue entry produced by the kernel.
	uint32_t const cq_produce = m_cq_k_produce.load(std::memory_order_acquire);

	if (cq_produce != m_cq_consume)
	{
		uint32_t const cq_mask = m_cq_size - 1;

		for (uint32_t cq_offset = cq_consume; cq_offset != cq_produce; ++cq_offset)
		{
			io_uring_cqe const& cqe = get_cqe(cq_offset & cq_mask);

			if (cqe.user_data == 0)
			{
				continue;
			}

			auto const user_data = vsm::reinterpret_pointer_cast<user_data_ptr>(cqe.user_data);

			switch (user_data.tag())
			{
			case user_data_tag::storage:
				{
					vsm_assert(user_data.ptr() != nullptr);
					auto& storage = static_cast<async_operation_storage&>(*user_data.ptr());
					storage.io_completed(*this, storage, cqe.res);
				}
				break;

			case user_data_tag::io_slot:
				{
					vsm_assert(user_data.ptr() != nullptr);
					auto& slot = static_cast<io_slot&>(*user_data.ptr());
					vsm_assert(slot.m_operation != nullptr);
					slot.m_operation->io_completed(*this, slot, cqe.res);
				}
				break;
			}
		}

		m_cq_k_consume.store(cq_produce, std::memory_order_release);
		m_cq_consume = cq_produce;
	}

	return cq_produce - cq_consume;
}

static vsm::result<void> partial_result(auto&& operation)
{
	bool made_progress = false;
	auto r = vsm_forward(operation)(made_progress);
	if (!r && !made_progress)
	{
		return r;
	}
	return {};
}

vsm::result<multiplexer::statistics> io_uring_multiplexer::pump(pump_parameters const& args)
{
	multiplexer::statistics statistics = {};
	vsm_try_void(partial_result([&](bool& made_progress) -> vsm::result<void>
	{
		bool enter_needed = false;
		bool enter_wanted = false;

		unsigned int enter_to_submit = 0;
		unsigned int enter_min_complete = 0;
		unsigned int enter_flags = 0;

		//TODO: Always reap submission queue?

		// Invoke any pending user listeners.
		if (vsm::any_flags(args.mode, pump_mode::flush))
		{
			made_progress |= deferring_multiplexer::flush(statistics);
		}

		// Submit previously recorded SQEs.
		if (vsm::any_flags(args.mode, pump_mode::submit))
		{
			if (vsm::any_flags(m_flags, flags::kernel_thread))
			{
				//TODO: Reap submission queue.
				//uint32_t const new_submission_count = reap_submission_queue_async();

				if (!vsm::any_flags(m_flags, flags::auto_submit))
				{
					if (needs_wakeup())
					{
						enter_needed = true;
					}

					enter_wanted = true;

					// Wake up sleeping kernel thread.
					enter_flags |= IORING_ENTER_SQ_WAKEUP;
				}
			}
			else
			{
				enter_wanted = true;

				// Submit as many as are available.
				enter_to_submit = static_cast<unsigned int>(-1);
			}
		}

		// Handle completed CQEs.
		if (vsm::any_flags(args.mode, pump_mode::complete))
		{
			uint32_t const new_completion_count = reap_completion_queue();

			if (new_completion_count == 0)
			{
				enter_wanted = true;

				// Wait for at least one completion.
				enter_min_complete = 1;

				// Get completions from the ring.
				enter_flags |= IORING_ENTER_GETEVENTS;
			}
		}

		if (enter_needed || enter_wanted && !made_progress)
		{
			__kernel_timespec enter_timespec;
			io_uring_getevents_arg enter_arg = {};
			io_uring_getevents_arg* p_enter_arg = nullptr;

			if (made_progress)
			{
				enter_min_complete = 0;
			}
			else if (args.deadline != deadline::never())
			{
				if (args.deadline == deadline::instant())
				{
					enter_min_complete = 0;
				}
				else if (vsm::any_flags(m_flags, flags::enter_ext_arg))
				{
					enter_flags |= IORING_ENTER_EXT_ARG;
					enter_timespec = make_timespec<__kernel_timespec>(args.deadline);
					enter_arg.ts = reinterpret_cast<uintptr_t>(&enter_timespec);
					p_enter_arg = &enter_arg;
				}
				else
				{
					return vsm::unexpected(error::unsupported_operation);
				}
			}

			int const enter_consumed = io_uring_enter(
				m_io_uring.get(),
				enter_to_submit,
				enter_min_complete,
				enter_flags,
				p_enter_arg,
				sizeof(enter_arg));

			if (enter_consumed == -1)
			{
				return vsm::unexpected(get_last_error());
			}

			// io_uring_enter returns the number of SQEs consumed by this call.
			if (enter_consumed > 0 && !vsm::any_flags(m_flags, flags::kernel_thread))
			{
				//uint32_t const old_sq_produce = m_sq_produce;
				//uint32_t const new_sq_produce = old_sq_produce + static_cast<uint32_t>(enter_consumed);
				//m_sq_produce = new_sq_produce;
			}
		}

		return {};
	}));
	return statistics;


	#if 0
	return gather_statistics([&](statistics& statistics) -> vsm::result<void>
	{
		bool need_enter = false;
		uint32_t enter_submission_count = 0;
		uint32_t enter_completion_count = 0;
		uint32_t enter_flags = 0;

		if (submit)
		{
			uint32_t const new_submission_count = reap_submission_queue();

			if (new_submission_count != 0)
			{
				if (!vsm::any_flags(m_flags, flags::kernel_thread))
				{
					need_enter = true;
					enter_submission_count = new_submission_count;
				}
				else if (needs_wakeup())
				{
					if (wrap_lt(m_sq_k_consume.load(std::memory_order_acquire), m_sq_submit))
					{
						need_enter = true;
						enter_flags |= IORING_ENTER_SQ_WAKEUP;
					}
				}
			}
		}

		if (complete)
		{
			uint32_t const new_completion_count = reap_completion_queue();

			//TODO: Return an error on pump when there are no operations in flight.

			if (new_completion_count == 0)
			{
				need_enter = true;
				enter_completion_count = 1;
				enter_flags |= IORING_ENTER_GETEVENTS;
			}
		}

		if (need_enter)
		{
			__kernel_timespec enter_timespec;
			io_uring_getevents_arg enter_arg = {};
			io_uring_getevents_arg* p_enter_arg = nullptr;

			if (args.deadline != deadline::never())
			{
				if (args.deadline == deadline::instant())
				{
					enter_completion_count = 0;
				}
				else if (vsm::any_flags(m_flags, flags::enter_ext_arg))
				{
					enter_flags |= IORING_ENTER_EXT_ARG;
					enter_timespec = make_timespec<decltype(enter_timespec)>(args.deadline);
					enter_arg.ts = reinterpret_cast<uintptr_t>(&enter_timespec);
					p_enter_arg = &enter_arg;
				}
				else
				{
					return vsm::unexpected(error::unsupported_operation);
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
				return vsm::unexpected(get_last_error());
			}

			if (submit)
			{

			}

			if (complete)
			{
				reap_completion_queue();
			}
		}

		return {};
	});
	#endif
}

allio_type_id(io_uring_multiplexer);
