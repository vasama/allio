#include <allio/linux/detail/io_uring/multiplexer.hpp>

#include <allio/impl/error_encoding.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/eventfd.hpp>
#include <allio/impl/linux/io_uring.hpp>
#include <allio/impl/linux/poll.hpp>
#include <allio/impl/linux/timeout.hpp>
#include <allio/impl/linux/version.hpp>
#include <allio/linux/io_uring_record_context.hpp>

#include <vsm/assert.h>
#include <vsm/defer.hpp>
#include <vsm/flags.hpp>
#include <vsm/lazy.hpp>
#include <vsm/numeric.hpp>
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
	//TODO: This shouldn't really be here... Move to linux default multiplexer instead.
	// The io_uring multiplexer requires at minimum Linux 5.5 for IORING_FEAT_NODROP.
	if (get_kernel_version() < KERNEL_VERSION(5, 5, 0))
	{
		return false;
	}

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
		vsm::truncating(offset));

	if (addr == MAP_FAILED)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	return vsm_lazy(unique_mmap<T>(reinterpret_cast<T*>(addr), mmap_deleter(size)));
}

static uint8_t get_cqe_skip_success_flag(io_uring_params const& setup)
{
	if (setup.features & IORING_FEAT_CQE_SKIP)
	{
		return IOSQE_CQE_SKIP_SUCCESS;
	}

	return 0;
}

static uint32_t* get_kernel_uint32(unique_byte_mmap const& mmap, size_t const offset)
{
	return reinterpret_cast<uint32_t*>(mmap.get() + offset);
}

_io_uring_multiplexer::_io_uring_multiplexer(
	io_uring_params const& setup,
	unique_handle&& io_uring,
	unique_byte_mmap&& sq_ring,
	unique_byte_mmap&& cq_ring,
	unique_void_mmap&& sq_data) noexcept
	: m_io_uring(vsm_move(io_uring))
	, m_sq_mmap(vsm_move(sq_ring))
	, m_cq_mmap(vsm_move(cq_ring))
	, m_sqes(vsm_move(sq_data))
	, m_cqes(m_cq_mmap.get() + setup.cq_off.cqes)

	, m_sqe_size(get_sqe_size(setup))
	, m_cqe_size(get_cqe_size(setup))
	, m_sqe_multiply_shift(vsm::truncating(std::countr_zero(m_sqe_size)))
	, m_cqe_multiply_shift(vsm::truncating(std::countr_zero(m_cqe_size)))
	, m_cqe_skip_success_flag(get_cqe_skip_success_flag(setup))

	, m_k_sq_produce(*get_kernel_uint32(m_sq_mmap, setup.sq_off.tail))
	, m_k_sq_consume(*get_kernel_uint32(m_sq_mmap, setup.sq_off.head))
	, m_k_sq_array(get_kernel_uint32(m_sq_mmap, setup.sq_off.array))
	, m_k_cq_produce(*get_kernel_uint32(m_cq_mmap, setup.cq_off.tail))
	, m_k_cq_consume(*get_kernel_uint32(m_cq_mmap, setup.cq_off.head))
	, m_k_flags(*get_kernel_uint32(m_sq_mmap, setup.sq_off.flags))

	, m_sq_size(setup.sq_entries)
	, m_sq_free(m_sq_size)
	, m_sq_consume(m_k_sq_consume.load(std::memory_order_relaxed))
	, m_sq_acquire(m_sq_consume)
	, m_sq_release(m_sq_acquire)

	, m_cq_size(setup.cq_entries)
	, m_cq_consume(m_k_cq_produce.load(std::memory_order_relaxed))
{
	if (setup.features & IORING_FEAT_EXT_ARG)
	{
		m_has_enter_ext_arg = true;
	}

	if (setup.flags & IORING_SETUP_SQPOLL)
	{
		m_has_kernel_thread = true;
	}

	for (uint32_t i = 0; i < m_sq_size; ++i)
	{
		m_k_sq_array[m_sq_acquire + i & m_sq_size - 1] = i;
	}
}

_io_uring_multiplexer::~_io_uring_multiplexer()
{
	if (m_registered_io_uring != -1)
	{
		io_uring_rsrc_update update =
		{
			.offset = static_cast<uint32_t>(m_registered_io_uring),
		};

		unrecoverable(vsm::discard_value(io_uring_register(
			/* fd: */ -1,
			IORING_UNREGISTER_RING_FDS,
			&update,
			/* nr_args: */ 1)));
	}
}

vsm::result<io_uring_multiplexer> io_uring_multiplexer::_create(
	create_parameters const& args) noexcept
{
	static constexpr uint32_t default_submission_queue_size = 32;

	// This places an upper limit on the length of SQE chains which submission is guaranteed to
	// succeed. If the chain is longer than the full length of the SQE ring, the operation fails.
	static constexpr uint32_t min_submission_queue_size = 4;

	// Round up to the next power of two no larger than 2^31.
	auto const round_up_to_power_of_two = [](uint32_t const value) -> vsm::result<uint32_t>
	{
		auto const lz = std::countl_zero(value);
		if (lz == 0 && (value & value - 1) != 0)
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
		}
		return static_cast<uint32_t>(1) << (31 - lz);
	};


	io_uring_params setup = {};

	uint32_t entries = default_submission_queue_size;

	if (args.submission_queue_size != 0)
	{
		if (args.submission_queue_size <= min_submission_queue_size)
		{
			entries = min_submission_queue_size;
		}
		else
		{
			if (vsm::loses_precision<uint32_t>(args.submission_queue_size))
			{
				return vsm::unexpected(allio_error(error::invalid_argument));
			}

			vsm_try_assign(entries, round_up_to_power_of_two(
				vsm::truncating(args.submission_queue_size)));
		}
	}

	if (args.completion_queue_size != 0)
	{
		if (args.submission_queue_size != 0 &&
			args.submission_queue_size > args.completion_queue_size)
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		if (args.completion_queue_size >= entries * 2)
		{
			if (vsm::loses_precision<uint32_t>(args.completion_queue_size))
			{
				return vsm::unexpected(allio_error(error::invalid_argument));
			}

			vsm_try_assign(setup.cq_entries, round_up_to_power_of_two(
				vsm::truncating(args.completion_queue_size)));

			setup.flags |= IORING_SETUP_CQSIZE;
		}
	}

	if (vsm::any_flags(args.options, io_uring_options::kernel_thread))
	{
		setup.flags |= IORING_SETUP_SQPOLL;

		if (args.kernel_thread != nullptr)
		{
			setup.wq_fd = static_cast<uint32_t>(
				args.kernel_thread->m_multiplexer->m_io_uring.get());
		}
	}

	vsm_try(io_uring, io_uring_setup(entries, setup));

	if ((setup.features & IORING_FEAT_NODROP) == 0)
	{
		// The nodrop feature is required. It is supported since Linux 5.5.
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

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

	int const kernel_version = get_kernel_version();

	auto multiplexer = std::make_unique<_io_uring_multiplexer>(
		setup,
		vsm_move(io_uring),
		vsm_move(rings.sq_ring),
		vsm_move(rings.cq_ring),
		vsm_move(sq_data));

	if (kernel_version >= KERNEL_VERSION(5, 18, 0) &&
		vsm::any_flags(args.options, io_uring_options::register_ring))
	{
		io_uring_rsrc_update update =
		{
			.offset = static_cast<uint32_t>(-1),
			.data = vsm::truncating(multiplexer->m_io_uring.get()),
		};

		int const r = _io_uring_register(
			/* fd: */ -1,
			IORING_REGISTER_RING_FDS,
			&update,
			/* nr_args: */ 1);

		if (r != -1)
		{
			multiplexer->m_registered_io_uring = static_cast<int>(update.offset);
		}
	}

	if (kernel_version < KERNEL_VERSION(6, 12, 0))
	{
		//TODO: Resubmit the poll operation if it ends due to CQE exhaustion.

		// Upon construction of the io_uring, a multishot poll operation is submitted on the wake
		// event. Any time the event becomes signaled, the completion wakes up the poll thread.

		vsm_try_assign(multiplexer->m_wake_event, eventfd(EFD_CLOEXEC | EFD_NONBLOCK));

		_io_uring_multiplexer::record_context ctx(*multiplexer);

		vsm_try_ptr(sqe, ctx.push());
		sqe =
		{
			.opcode = IORING_OP_POLL_ADD,
			.fd = multiplexer->m_wake_event.get(),
			.len = IORING_POLL_ADD_MULTI,
			.poll_events = POLLIN,
		};

		ctx.commit();
	}

	return vsm::result<io_uring_multiplexer>(
		vsm::result_value,
		vsm_lazy(io_uring_multiplexer(vsm_move(multiplexer))));
}


vsm::result<void> _io_uring_multiplexer::attach_fd(int const fd, connector_type& c)
{
	if (fd == -1)
	{
		return vsm::unexpected(allio_error(error::handle_is_null));
	}

	//TODO: Implement registered files.
	c.file_index = -1;

	return {};
}

vsm::result<void> _io_uring_multiplexer::detach_fd(int const fd, connector_type& c)
{
	if (fd == -1)
	{
		return vsm::unexpected(allio_error(error::handle_cannot_be_detached));
	}

	return {};
}


bool _io_uring_multiplexer::is_kernel_thread_inactive() const
{
	return m_k_flags.load(std::memory_order_acquire) & IORING_SQ_NEED_WAKEUP;
}


bool _io_uring_multiplexer::has_available_sqes() const
{
	return m_sq_acquire != m_sq_consume;
}

bool _io_uring_multiplexer::has_pending_sqes() const
{
	return m_sq_release != m_sq_acquire;
}

void _io_uring_multiplexer::release_sqes()
{
	m_k_sq_produce.store(m_sq_acquire, std::memory_order_release);
	m_sq_release = m_sq_acquire;
}

bool _io_uring_multiplexer::acquire_cqes()
{
	uint32_t const old_cq_produce = m_cq_produce;
	uint32_t const new_cq_produce = m_k_cq_produce.load(std::memory_order_acquire);

	m_cq_produce = new_cq_produce;
	return new_cq_produce != old_cq_produce;
}

bool _io_uring_multiplexer::has_pending_cqes() const
{
	return m_cq_consume != m_cq_produce;
}

void _io_uring_multiplexer::reap_cqe(io_uring_cqe const& cqe)
{
	if (cqe.user_data == 0)
	{
		return;
	}

	auto const user_data = vsm::reinterpret_pointer_cast<user_data_ptr>(cqe.user_data);
	vsm_assert(user_data.ptr() != nullptr);

	auto const tag = user_data.tag();

	// Emulate IOSQE_CQE_SKIP_SUCCESS for CQEs canceled via links, if requested. This avoids needing
	// to delay completion of the operation until all linked CQEs still referencing their associated
	// io_slot have been reaped.
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

	operation_type* operation;

	if (vsm::any_flags(tag, user_data_tag::io_slot))
	{
		io_slot* const slot = static_cast<io_slot*>(user_data.ptr());

		// Emulate IOSQE_CQE_SKIP_SUCCESS for successful operations.
		if (status.result >= 0 && vsm::any_flags(slot->m_flags, io_slot_flags::cqe_skip_success))
		{
			return;
		}

		status.slot = slot;

		operation = slot->get_operation();
	}
	else
	{
		operation = static_cast<operation_type*>(user_data.ptr());
	}

	io_handler_ptr handler = operation->load(std::memory_order_acquire);
	if (handler.tag() != io_handler_tag::cancel_pending)
	{
		while (true)
		{
			(void)flush_cancel_queue();

			handler = operation->load(std::memory_order_acquire);
			if (handler.tag() != io_handler_tag::cancel_pending)
			{
				break;
			}

			std::this_thread::yield();
		}
	}

	// The CQE becomes potentially invalid upon the call to notify.
	handler->notify(vsm_move(status));
}

bool _io_uring_multiplexer::reap_all_cqes()
{
	uint32_t const cq_consume = m_cq_consume;
	uint32_t const cq_produce = m_cq_produce;

	if (cq_consume == cq_produce)
	{
		return false;
	}

	ring_view<io_uring_cqe const> const cqes = get_cqes();

	for (uint32_t cq_offset = cq_consume; cq_offset != cq_produce; ++cq_offset)
	{
		reap_cqe(cqes[cq_offset]);

		// Release this CQE back to the kernel as early as possible.
		m_k_cq_consume.store(cq_offset + 1, std::memory_order_release);
	}

	m_cq_consume = cq_produce;

	// In case a wake CQE was posted, the wake flag must be reset for the next time a wake up is
	// required.
	wake_poll_thread_reset();

	return true;
}

void _io_uring_multiplexer::submit_async_cancel(user_data_ptr const user_data)
{
	_io_uring_multiplexer::record_context ctx(*this);

	if (auto const r = ctx.push())
	{
		**r =
		{
			.opcode = IORING_OP_ASYNC_CANCEL,
			.addr = vsm::reinterpret_pointer_cast<uintptr_t>(user_data),
		};

		ctx.commit();
	}
	else
	{
		unrecoverable_error(r.error());
	}
}

void _io_uring_multiplexer::cancel_io_externally_synchronized(
	operation_type& operation,
	user_data_ptr const user_data)
{
	io_handler_ptr handler = operation.load(std::memory_order_acquire);

	/**/ if (handler.tag() == io_handler_tag::not_cancelled)
	{
		static_assert(vsm::all_flags(
			static_cast<uintptr_t>(io_handler_tag::cancel_pending),
			static_cast<uintptr_t>(io_handler_tag::cancel_submitted)));

		// This races with calls to cancel_io_internally_synchronized. Any such thread will attempt
		// to set the tag to cancel_pending. Should one do so between the previous load and this
		// operation, setting the cancel_submitted bits will have no effect, because, as asserted
		// above, the cancel_pending value sets all the bits of the cancel_submitted value.
		handler = operation.fetch_or(io_handler_tag::cancel_submitted, std::memory_order_acq_rel);

		// Another thread could have won the race to modify the tag, in which case it must now be in
		// the cancel_pending state. The other thread will push the operation into the cancel queue.
		if (handler.tag() == io_handler_tag::cancel_pending)
		{
			return;
		}

		vsm_assert(handler.tag() == io_handler_tag::not_cancelled);
	}
	else if (handler.tag() == io_handler_tag::cancel_flushing)
	{
		operation.store(
			{ handler.ptr(), io_handler_tag::cancel_submitted },
			std::memory_order_relaxed);
	}
	else
	{
		return;
	}

	submit_async_cancel(user_data);
}

void _io_uring_multiplexer::cancel_io_internally_synchronized(operation_type& operation)
{
	io_handler_ptr handler = operation.load(std::memory_order_acquire);

	if (handler.tag() != io_handler_tag::not_cancelled)
	{
		return;
	}

	(void)operation.compare_exchange_strong(
		handler,
		{ handler.ptr(), io_handler_tag::cancel_pending },
		std::memory_order_release,
		std::memory_order_relaxed);

	if (handler.tag() != io_handler_tag::not_cancelled)
	{
		return;
	}

	if (m_shared.cancel_queue.push_back(operation))
	{
		// Checking for the cancel pending flag avoids doing a syscall when the cancellation of
		// another operation has already woken up the polling thread and it has yet to react.
		if (!m_shared.cancel_pending.load(std::memory_order_acquire) &&
			!m_shared.cancel_pending.exchange(true, std::memory_order_acq_rel))
		{
			wake_poll_thread();
		}
	}
}

void _io_uring_multiplexer::cancel_io(operation_type& operation, user_data_ptr const user_data)
{
	if (is_externally_synchronized())
	{
		cancel_io_externally_synchronized(operation, user_data);
	}
	else
	{
		cancel_io_internally_synchronized(operation);
	}
}

void _io_uring_multiplexer::wake_poll_thread()
{
	if (!m_shared.wake_requested.load(std::memory_order_acquire) &&
		!m_shared.wake_requested.exchange(true, std::memory_order_acq_rel))
	{
		if (m_wake_event)
		{
			// Signal the continuously polled wake event in order to wake up the poll thread if it
			// happens to be waiting on io_uring_enter.
			unrecoverable(eventfd_signal(m_wake_event.get()));
		}
		else
		{
			// Wake the polling thread by directly sending a completion to the ring.

			io_uring_sqe sqe =
			{
				.fd = m_io_uring.get(),
			};

			unrecoverable(vsm::discard_value(io_uring_register(
				/* fd: */ -1,
				IORING_REGISTER_SEND_MSG_RING,
				&sqe,
				/* nr_args: */ 1)));
		}
	}
}

void _io_uring_multiplexer::wake_poll_thread_reset()
{
	if (m_shared.wake_requested.load(std::memory_order_acquire))
	{
		do
		{
			// Before resetting the atomic flag, the event object must be reset.
			unrecoverable(vsm::discard_value(eventfd_reset(m_wake_event.get())));
		}
		while (m_shared.wake_requested.exchange(false, std::memory_order_acq_rel));
	}
}

bool _io_uring_multiplexer::flush_cancel_queue()
{
	vsm_assert(is_externally_synchronized());

	if (!m_shared.cancel_pending.load(std::memory_order_acquire))
	{
		return false;
	}

	do
	{
		// Popping the queue elements in reversed order avoids reversing the list eagerly.
		auto list = m_shared.cancel_queue.pop_all_reversed();

		auto beg = list.begin();
		auto const end = list.end();

		while (beg != end)
		{
			operation_type& operation = *beg++;

			// This thread is currently not racing with any modification of the handler. The only
			// other modification by another thread is in cancel_io_internally_synchronized and it
			// loads and checks the value before modifying it, never modifying if cancellation has
			// already been initiated. For this reason relaxed memory order is sufficient.
			io_handler_ptr handler = operation.load(std::memory_order_relaxed);

			// The handler tag must be cancel_pending before the operation can end up in the queue.
			vsm_assert(handler.tag() == io_handler_tag::cancel_pending);

			operation.store(
				{ handler.ptr(), io_handler_tag::cancel_flushing },
				std::memory_order_relaxed);

			handler->cancel();

			handler = operation.load(std::memory_order_relaxed);
			if (handler.tag() == io_handler_tag::cancel_flushing)
			{
				operation.store(
					{ handler.ptr(), io_handler_tag::cancel_submitted },
					std::memory_order_relaxed);
			}
		}
	}
	while (m_shared.cancel_pending.exchange(false, std::memory_order_acq_rel));

	return true;
}

vsm::result<int> _io_uring_multiplexer::enter(
	unsigned const to_submit,
	unsigned min_complete,
	unsigned flags,
	deadline const deadline)
{
	__kernel_timespec timespec;
	io_uring_getevents_arg arg = {};
	io_uring_getevents_arg* p_arg = nullptr;

	if (deadline != deadline::never())
	{
		if (deadline == deadline::instant())
		{
			min_complete = 0;
		}
		else if (m_has_enter_ext_arg)
		{
			// If IORING_FEAT_EXT_ARG is available, a timeout can be specified using the
			// extended io_uring_getevents_arg parameter structure.
			timespec = make_timespec<__kernel_timespec>(deadline);
			arg.ts = reinterpret_cast<uintptr_t>(&timespec);
			flags |= IORING_ENTER_EXT_ARG;
			p_arg = &arg;
		}
		else if (min_complete != 0)
		{
			// Submitting a timeout operation is possible, but the timespec lifetime and
			// cancellation required make it very complicated. Instead we'll just fall back to
			// polling the io_uring object.
			vsm_try_discard(linux::poll(m_io_uring.get(), POLLIN, deadline));

			min_complete = 1;
		}
	}

	int io_uring_fd = m_io_uring.get();
	if (m_registered_io_uring != -1)
	{
		io_uring_fd = m_registered_io_uring;
		flags |= IORING_ENTER_REGISTERED_RING;
	}

	int const r = _io_uring_enter(
		io_uring_fd,
		to_submit,
		min_complete,
		flags,
		p_arg,
		sizeof(arg));

	if (r == -1)
	{
		switch (int const e = errno)
		{
		case EAGAIN:
			return EAGAIN;

		case ETIME:
			return ETIME;

		case EBADR:
			// The kernel has dropped a completion despite IORING_FEAT_NODROP. There is no possible
			// way for the library to recover from this condition.
			std::abort();

		default:
			return vsm::unexpected(allio_error(static_cast<system_error>(e)));
		}
	}

	//TODO: Do we need to handle consumed if kernel thread is not being used?

	m_sq_consume = m_k_sq_consume.load(std::memory_order_acquire);

	return 0;
}

vsm::result<void> _io_uring_multiplexer::wait_for_sqe(deadline const deadline)
{
	unsigned enter_flags = 0;

	if (has_pending_sqes())
	{
		release_sqes();
	}

	if (m_has_kernel_thread)
	{
		enter_flags |= IORING_ENTER_SQ_WAIT;

		if (is_kernel_thread_inactive())
		{
			enter_flags |= IORING_ENTER_SQ_WAKEUP;
		}
	}

	vsm_try(error, enter(
		/* to_submit: */ static_cast<unsigned>(-1),
		/* min_complete: */ 0,
		enter_flags,
		deadline));

	if (error)
	{
		//TODO: Handle EAGAIN?

		return vsm::unexpected(allio_error(static_cast<system_error>(error)));
	}

	return {};
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

template<typename ExternallySynchronized>
class optional_scoped_synchronization
{
	std::optional<scoped_synchronization<ExternallySynchronized>> m_object;

public:
	optional_scoped_synchronization(ExternallySynchronized& object)
	{
		if (!object.is_externally_synchronized())
		{
			m_object.emplace(object);
		}
	}
};

} // namespace

vsm::result<bool> _io_uring_multiplexer::poll(poll_parameters const& args)
{
	optional_scoped_synchronization const synchronization(*this);

	using reason = enter_reason;

	enter_reason enter_reason = reason::none;
	unsigned int enter_to_submit = 0;
	unsigned int enter_min_complete = 0;
	unsigned int enter_flags = 0;

	bool made_progress = false;

	made_progress |= flush_cancel_queue();

	if (has_pending_sqes())
	{
		// Release ready SQEs to the kernel.
		release_sqes();

		if (m_has_kernel_thread)
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

		made_progress = true;
	}

	// Check for new CQEs produced by the kernel. If there are any, entering will not be necessary.
	// In any case, we want to enter before reaping the completions in order to prioritize
	// submissions. If there are no pending CQEs, enter the kernel and wait.
	if (!acquire_cqes() && !has_pending_cqes())
	{
		enter_min_complete = 1;
		//TODO: Should this flag always be set?
		enter_flags |= IORING_ENTER_GETEVENTS;
		enter_reason |= reason::wait_for_cqes;
	}

	if (enter_reason != reason::none)
	{
		vsm_try(error, enter(enter_to_submit, enter_min_complete, enter_flags, args.deadline));

		switch (error)
		{
		case 0:
			break;

		case EAGAIN:
			break;

		case ETIME:
			return made_progress;

		default:
			return vsm::unexpected(allio_error(static_cast<system_error>(error)));
		}

		// Check for new CQEs produced by the kernel.
		made_progress |= acquire_cqes();
	}

	// Finally, reap the pending CQEs, invoking their notify callbacks if necessary.
	return made_progress | reap_all_cqes();
}
