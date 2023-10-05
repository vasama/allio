#include <allio/linux/detail/io_uring/multiplexer.hpp>

#include <allio/impl/linux/kernel/io_uring.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/timeout.hpp>
#include <allio/linux/io_uring_record_context.hpp>

#include <vsm/assert.h>
#include <vsm/flags.hpp>
#include <vsm/lazy.hpp>
#include <vsm/tag_ptr.hpp>
#include <vsm/utility.hpp>

#include <linux/time_types.h>
#include <poll.h>
#include <sys/mman.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

bool io_uring::is_supported()
{
	static constexpr uint8_t lazy_init_flag = 0x80;
	static constexpr uint8_t supported_flag = 0x01;

	static constinit uint8_t storage = 0;
	auto const reference = vsm::atomic_ref(storage);

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


static uint8_t get_sqe_size(io_uring_params const& setup)
{
	static_assert(sizeof(io_uring_sqe) < 0x100);
	return setup.flags & IORING_SETUP_SQE128 ? 128 : sizeof(io_uring_sqe);
}

static uint8_t get_cqe_size(io_uring_params const& setup)
{
	static_assert(sizeof(io_uring_cqe) < 0x100);
	return setup.flags & IORING_SETUP_CQE32 ? 32 : sizeof(io_uring_cqe);
}

template<typename T = std::byte>
static vsm::result<unique_mmap<T>> mmap(int const fd, uint64_t const offset, size_t const size)
{
	void* const addr = ::mmap(
		nullptr,
		size,
		PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_POPULATE,
		fd,
		offset);

	if (addr == MAP_FAILED)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm_lazy(unique_mmap<T>(reinterpret_cast<T*>(addr), mmap_deleter(size)));
}

vsm::result<io_uring_multiplexer> io_uring_multiplexer::_create(io_parameters_t<create_t> const& args)
{
	static constexpr uint32_t min_queue_entries = 32;

	auto const truncate_to_uint32 = [](std::unsigned_integral auto const value) -> vsm::result<uint32_t>
	{
		if (value > std::numeric_limits<uint32_t>::max())
		{
			return vsm::unexpected(error::invalid_argument);
		}

		return static_cast<uint32_t>(value);
	};

	// Round up to the next power of two no larger than 2^31.
	// If the 
	auto const round_up_to_power_of_two = [](uint32_t const value) -> vsm::result<uint32_t>
	{
		uint32_t const lz = std::countl_zero(value);
		if (lz == 0 && (value & value - 1) != 0)
		{
			return vsm::unexpected(error::invalid_argument);
		}
		return static_cast<uint32_t>(1) << (31 - lz);
	};

	vsm_try(submission_queue_size, truncate_to_uint32(args.submission_queue_size));
	vsm_try(completion_queue_size, truncate_to_uint32(args.completion_queue_size));

	vsm_try(min_entries, round_up_to_power_of_two(std::max(
		min_queue_entries,
		std::max(submission_queue_size, completion_queue_size))));

	io_uring_params setup = {};

	if (args.kernel_thread)
	{
		setup.flags |= IORING_SETUP_SQPOLL;

		if (*args.kernel_thread != nullptr)
		{
			setup.wq_fd = (*args.kernel_thread)->m_io_uring.get();
		}
	}

	vsm_try(io_uring, io_uring_setup(min_entries, setup));

	struct ring_pair
	{
		unique_byte_mmap sq_ring;
		unique_byte_mmap cq_ring;
	};

	vsm_try(rings, [&]() -> vsm::result<ring_pair>
	{
		size_t sq_size = setup.sq_off.array + setup.sq_entries + sizeof(uint32_t);
		size_t cq_size = setup.cq_off.cqes + setup.cq_entries * get_cqe_size(setup);

		if (setup.features & IORING_FEAT_SINGLE_MMAP)
		{
			sq_size = cq_size = std::max(sq_size, cq_size);
		}

		vsm_try(sq_ring, mmap(io_uring.get(), IORING_OFF_SQ_RING, sq_size));

		vsm_try(cq_ring, [&]() -> vsm::result<unique_byte_mmap>
		{
			if (setup.features & IORING_FEAT_SINGLE_MMAP)
			{
				return vsm_lazy(unique_byte_mmap(
					sq_ring.get(),
					mmap_deleter::borrow()));
			}

			return mmap(io_uring.get(), IORING_OFF_CQ_RING, cq_size);
		}());

		return vsm_lazy(ring_pair
		{
			.sq_ring = vsm_move(sq_ring),
			.cq_ring = vsm_move(cq_ring),
		});
	}());

	vsm_try(sq_data, mmap<void>(
		io_uring.get(),
		IORING_OFF_SQES,
		setup.sq_entries * get_sqe_size(setup)));

	return vsm_lazy(io_uring_multiplexer(
		vsm_move(io_uring),
		vsm_move(rings.sq_ring),
		vsm_move(rings.cq_ring),
		vsm_move(sq_data),
		setup));
}

static uint32_t* get_shared_uint32(unique_byte_mmap const& mmap, size_t const offset)
{
	return reinterpret_cast<uint32_t*>(mmap.get() + offset);
}

io_uring_multiplexer::io_uring_multiplexer(
	unique_fd&& io_uring,
	unique_byte_mmap&& sq_ring,
	unique_byte_mmap&& cq_ring,
	unique_void_mmap&& sq_data,
	io_uring_params const& setup)
	: m_io_uring(vsm_move(io_uring))
	, m_sq_mmap(vsm_move(sq_ring))
	, m_cq_mmap(vsm_move(cq_ring))
	, m_sqes(vsm_move(sq_data))
	, m_cqes(m_cq_mmap.get() + setup.cq_off.cqes)

	, m_flags{}
	, m_sqe_size(get_sqe_size(setup))
	, m_cqe_size(get_cqe_size(setup))
	, m_sqe_multiply_shift(std::countr_zero(m_sqe_size))
	, m_cqe_multiply_shift(std::countr_zero(m_cqe_size))
	, m_cqe_skip_success(0)

	, m_k_sq_produce(*get_shared_uint32(m_sq_mmap, setup.sq_off.tail))
	, m_k_sq_consume(*get_shared_uint32(m_sq_mmap, setup.sq_off.head))
	, m_k_sq_array(get_shared_uint32(m_sq_mmap, setup.sq_off.array))
	, m_k_cq_produce(*get_shared_uint32(m_cq_mmap, setup.cq_off.tail))
	, m_k_cq_consume(*get_shared_uint32(m_cq_mmap, setup.cq_off.head))
	, m_k_flags(*get_shared_uint32(m_sq_mmap, setup.sq_off.flags))

	, m_sq_size(setup.sq_entries)
	, m_sq_free(m_sq_size)
	, m_sq_consume(m_k_sq_consume.load(std::memory_order_relaxed))
	, m_sq_acquire(m_sq_consume)
	, m_sq_release(m_sq_acquire)

	, m_cq_size(setup.cq_entries)
	, m_cq_free(m_cq_size)
	, m_cq_consume(m_k_cq_produce.load(std::memory_order_relaxed))
{
	if (setup.features & IORING_FEAT_EXT_ARG)
	{
		m_flags |= flags::enter_ext_arg;
	}

	if (setup.flags & IORING_SETUP_SQPOLL)
	{
		m_flags |= flags::kernel_thread;
	}

	if (setup.features & IORING_FEAT_CQE_SKIP)
	{
		m_cqe_skip_success = IOSQE_CQE_SKIP_SUCCESS;
	}

	for (size_t i = 0; i < m_sq_size; ++i)
	{
		m_k_sq_array[m_sq_acquire + i & m_sq_size - 1] = i;
	}
}


vsm::result<void> io_uring_multiplexer::attach_handle(native_platform_handle const handle, connector_type& c)
{
	//TODO: Implement registered files.
	c.file_index = -1;

	return {};
}

void io_uring_multiplexer::detach_handle(native_platform_handle const handle, connector_type& c)
{
}


#if 0
vsm::result<void> io_uring_multiplexer::record_context::push_linked_timeout(timeout::reference const timeout)
{
#if __cpp_lib_is_layout_compatible
	static_assert(std::is_layout_compatible_v<timeout::timespec, __kernel_timespec>);
#else
	static_assert(sizeof(timeout::timespec) == sizeof(__kernel_timespec));
	static_assert(alignof(timeout::timespec) == alignof(__kernel_timespec));
#endif

	//TODO: Make sure linked timeouts never produce a CQE.

	bool const skip_success = vsm::any_flags(m_flags, flags::cqe_skip_success);
	vsm_try(sqe, acquire_sqe(/* acquire_cqe: */ !skip_success));

	sqe.opcode = IORING_OP_LINK_TIMEOUT;
	sqe.flags = IOSQE_IO_LINK | (skip_success ? IOSQE_CQE_SKIP_SUCCESS : 0);
	sqe.addr = reinterpret_cast<uintptr_t>(&timeout.m_timeout->m_timespec);
	sqe.len = 1;
	sqe.timeout_flags = timeout.m_absolute
		? IORING_TIMEOUT_ABS
		: 0;

	return {};
}

bool io_uring_multiplexer::record_context::check_sqe_flags(io_uring_sqe const& sqe) noexcept
{
	return true;
}

bool io_uring_multiplexer::record_context::check_sqe_user_data(io_uring_sqe const& sqe, io_data_ref const data) noexcept
{
	return sqe.user_data == reinterpret_pointer_cast<uintptr_t>(data);
}

vsm::result<io_uring_sqe*> io_uring_multiplexer::record_context::acquire_sqe(io_data_ref const data)
{
	vsm_try(sqe, acquire_sqe(/* acquire_cqe: */ true));
	sqe->user_data = reinterpret_pointer_cast<uintptr_t>(data);
	return sqe;
}

vsm::result<io_uring_sqe*> io_uring_multiplexer::record_context::acquire_sqe(bool const acquire_cqe)
{
	if (m_sq_acquire == m_multiplexer.m_sq_consume)
	{
		uint32_t const k_sq_consume = m_multiplexer.m_k_sq_consume.load(std::memory_order_acquire);

		if (k_sq_consume == m_multiplexer.m_sq_consume)
		{
			return vsm::unexpected(error::too_many_concurrent_async_operations);
		}

		m_multiplexer.m_sq_consume = k_sq_consume;
	}

	if (acquire_cqe)
	{
		if (m_cq_free == 0)
		{
			return vsm::unexpected(error::too_many_concurrent_async_operations);
		}
		--m_cq_free;
	}

	return &m_multiplexer.get_sqe(m_multiplexer.get_sqe_offset(m_sq_acquire++));
}

vsm::result<void> io_uring_multiplexer::record_context::commit()
{
	m_multiplexer.m_sq_acquire = m_sq_acquire;
	m_multiplexer.m_cq_free = m_cq_free;
	return m_multiplexer.commit();
}
#endif


vsm::result<void> io_uring_multiplexer::commit()
{
	if (vsm::any_flags(m_flags, flags::auto_submit))
	{
		release_sqes();

		unsigned int enter_to_submit = 0;
		unsigned int enter_flags = 0;

		if (has_kernel_thread())
		{
			enter_flags |= IORING_ENTER_SQ_WAKEUP;
		}
		else
		{
			enter_to_submit = static_cast<unsigned>(-1);
		}

		vsm_try_discard(io_uring_enter(
			m_io_uring.get(),
			enter_to_submit,
			/* min_complete: */ 0,
			enter_flags,
			/* arg: */ nullptr));
	}

	return {};
}


#if 0
void io_uring_multiplexer::wake_kernel_thread()
{
	vsm_assert(has_kernel_thread()); //PRECONDITION

	if (kernel_thread_needs_wakeup())
	{
		int const r = io_uring_enter(
			m_io_uring.get(),
			/* to_submit: */ 0,
			/* min_complete: */ 0,
			IORING_ENTER_SQ_WAKEUP,
			/* arg: */ nullptr,
			/* argsz: */ 0);

		unrecoverable(get_last_error());
	}
}
#endif


bool io_uring_multiplexer::is_kernel_thread_inactive() const
{
	return m_k_flags.load(std::memory_order_acquire) & IORING_SQ_NEED_WAKEUP;
}


bool io_uring_multiplexer::has_pending_sqes() const
{
	return m_sq_release != m_sq_acquire;
}

void io_uring_multiplexer::release_sqes()
{
	m_k_sq_produce.store(m_sq_acquire, std::memory_order_release);
	m_sq_release = m_sq_acquire;
}

void io_uring_multiplexer::acquire_cqes()
{
	m_cq_produce = m_k_cq_produce.load(std::memory_order_acquire);
}

bool io_uring_multiplexer::has_pending_cqes() const
{
	return m_cq_consume != m_cq_produce;
}

bool io_uring_multiplexer::reap_all_cqes()
{
	uint32_t const cq_consume = m_cq_consume;
	uint32_t const cq_produce = m_cq_produce;

	if (cq_consume == cq_produce)
	{
		return false;
	}
	
	ring_view<io_uring_cqe> const cqes = get_cqes();

	for (uint32_t cq_offset = cq_consume; cq_offset != cq_produce; ++cq_offset)
	{
		reap_cqe(cqes[cq_offset]);

		// Release this CQE back to the kernel as early as possible.
		m_k_cq_consume.store(cq_offset, std::memory_order_release);
	}

	m_cq_consume = cq_produce;

	return true;
}

void io_uring_multiplexer::reap_cqe(io_uring_cqe const& cqe)
{
	// Whatever happens, after this function returns the CQE is free to be used again.
	++m_cq_free;

	if (cqe.user_data == 0)
	{
		return;
	}

	auto const user_data = vsm::reinterpret_pointer_cast<user_data_ptr>(cqe.user_data);
	vsm_assert(user_data.ptr() != nullptr);

	auto const tag = user_data.tag();

	// Emulate IOSQE_CQE_SKIP_SUCCESS for CQEs cancelled via links, if requested.
	// This avoids needing to delay completion of the operation until all
	// linked CQEs still referencing their associated io_data have been reaped.
	if (cqe.res == -ECANCELED && vsm::any_flags(tag, user_data_tag::cqe_skip_cancel))
	{
		return;
	}

	io_status_type status =
	{
		.slot = nullptr,
		.result = cqe.res,
		.flags = cqe.flags,
	};
	auto operation = static_cast<operation_type*>(user_data.ptr());

	if (vsm::any_flags(tag, user_data_tag::io_slot))
	{
		status.slot = static_cast<io_slot*>(user_data.ptr());
		auto const op_tag = status.slot->m_operation.tag();

		// Emulate IOSQE_CQE_SKIP_SUCCESS for successful operations.
		if (status.result >= 0 && vsm::any_flags(op_tag, operation_tag::cqe_skip_success))
		{
			return;
		}

		operation = status.slot->m_operation.ptr();
	}

	operation->notify(io_status(status));
}


namespace {

enum class enter_reason : uint32_t
{
	none                        = 0,
	submit_sqes                 = 1 << 0,
	wait_for_cqes               = 1 << 1,
	wake_kernel_thread          = 1 << 2,
};
vsm_flag_enum(enter_reason);

} // namespace

vsm::result<bool> io_uring_multiplexer::_poll(io_parameters_t<poll_t> const& args)
{
	using reason = enter_reason;

	enter_reason enter_reason = reason::none;
	unsigned int enter_to_submit = 0;
	unsigned int enter_min_complete = 0;
	unsigned int enter_flags = 0;

	// If the auto submit mode is not enabled, submit all pending SQEs now.
	if (!vsm::any_flags(m_flags, flags::auto_submit) && has_pending_sqes())
	{
		// Release ready SQEs to the kernel.
		release_sqes();

		if (has_kernel_thread())
		{
			if (is_kernel_thread_inactive())
			{
				enter_flags |= IORING_ENTER_SQ_WAKEUP;
				enter_reason |= reason::wake_kernel_thread;
			}
		}
		else
		{
			// Submit as many as are available.
			enter_to_submit = static_cast<unsigned>(-1);
			enter_reason |= reason::submit_sqes;
		}
	}

	// Check for new CQEs produced by the kernel.
	// If there are any, entering will not be necessary.
	// In any case, we want to enter before reaping the
	// completions in order to prioritize submissions.
	acquire_cqes();

	// If there are no pending CQEs, enter the kernel and wait.
	if (!has_pending_cqes())
	{
		enter_min_complete = 1;
		enter_flags |= IORING_ENTER_GETEVENTS;
		enter_reason |= reason::wait_for_cqes;
	}

	if (enter_reason != reason::none)
	{
		__kernel_timespec enter_timespec;
		io_uring_getevents_arg enter_arg = {};
		io_uring_getevents_arg* p_enter_arg = nullptr;
		bool need_poll = false;

		if (args.deadline != deadline::never())
		{
			if (args.deadline == deadline::instant())
			{
				enter_min_complete = 0;

				// With an instant timeout, we won't be waiting anyway,
				// so waiting for CQEs is no longer a reason to enter the kernel.
				enter_reason &= ~reason::wait_for_cqes;
			}
			else if (vsm::any_flags(m_flags, flags::enter_ext_arg))
			{
				// If IORING_FEAT_EXT_ARG is available, a timeout can be specified
				// using the extended io_uring_getevents_arg parameter structure.
				enter_flags |= IORING_ENTER_EXT_ARG;
				enter_timespec = make_timespec<__kernel_timespec>(args.deadline);
				enter_arg.ts = reinterpret_cast<uintptr_t>(&enter_timespec);
				p_enter_arg = &enter_arg;
			}
			else
			{
				// Submitting a timeout operation is possible, but the timespec
				// lifetime and cancellation required make it very complicated.
				// Instead we'll just fall back to polling the io_uring object.
				need_poll = true;

				// Since we're polling the io_uring, we no longer need to
				// enter the kernel in order to wait for CQEs.
				enter_reason &= ~reason::wait_for_cqes;
			}
		}

		if (enter_reason != reason::none)
		{
			vsm_try(consumed, io_uring_enter(
				m_io_uring.get(),
				enter_to_submit,
				enter_min_complete,
				enter_flags,
				p_enter_arg));

			//TODO: Do we need to handle consumed if kernel thread is not being used?

			// Check for new CQEs produced by the kernel.
			acquire_cqes();

			if (has_pending_cqes())
			{
				// If we entered for some reason other than waiting for CQEs and
				// new CQEs did unexpectedly become available, there is no longer
				// any need to fall back to polling the io_uring.
				need_poll = false;
			}
		}

		// Wait for new CQEs by polling the io_uring.
		if (need_poll)
		{
			pollfd poll_fd =
			{
				.fd = m_io_uring.get(),
				.events = POLLIN,
			};

			int const r = ppoll(
				&poll_fd,
				/* nfds: */ 1,
				kernel_timeout<timespec>(args.deadline),
				/* sigmask: */ nullptr);

			if (r == -1)
			{
				return vsm::unexpected(get_last_error());
			}

			if (r != 0)
			{
				// Check for new CQEs produced by the kernel.
				acquire_cqes();

				// The poll just completed successfully,
				// so there better be some pending CQEs.
				vsm_assert(has_pending_cqes());
			}
		}
	}

	// Finally, reap the pending CQEs, calling their io_callbacks if necessary.
	return reap_all_cqes();
}
