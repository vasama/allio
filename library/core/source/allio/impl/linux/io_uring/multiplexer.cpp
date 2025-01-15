#include <allio/linux/detail/io_uring/multiplexer.hpp>

#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/eventfd.hpp>
#include <allio/impl/linux/io_uring.hpp>
#include <allio/impl/linux/poll.hpp>
#include <allio/impl/linux/timeout.hpp>
#include <allio/linux/io_uring_record_context.hpp>

#include <vsm/assert.h>
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
		return vsm::unexpected(get_last_error());
	}

	return vsm_lazy(unique_mmap<T>(reinterpret_cast<T*>(addr), mmap_deleter(size)));
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
	, m_cqe_skip_success(0)

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
	, m_cq_free(m_cq_size)
	, m_cq_consume(m_k_cq_produce.load(std::memory_order_relaxed))

	, m_cancel_list_end(m_operation_list.end())
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

	for (uint32_t i = 0; i < m_sq_size; ++i)
	{
		m_k_sq_array[m_sq_acquire + i & m_sq_size - 1] = i;
	}
}

vsm::result<io_uring_multiplexer> io_uring_multiplexer::_create(
	create_parameters const& args) noexcept
{
	static constexpr uint32_t min_queue_entries = 32;

	// Round up to the next power of two no larger than 2^31.
	auto const round_up_to_power_of_two = [](uint32_t const value) -> vsm::result<uint32_t>
	{
		auto const lz = std::countl_zero(value);
		if (lz == 0 && (value & value - 1) != 0)
		{
			return vsm::unexpected(error::invalid_argument);
		}
		return static_cast<uint32_t>(1) << (31 - lz);
	};

	vsm_try(submission_queue_size, vsm::try_truncate<uint32_t>(
		args.submission_queue_size,
		error::invalid_argument));

	vsm_try(completion_queue_size, vsm::try_truncate<uint32_t>(
		args.completion_queue_size,
		error::invalid_argument));

	vsm_try(min_entries, round_up_to_power_of_two(std::max(
		min_queue_entries,
		std::max(submission_queue_size, completion_queue_size))));

	io_uring_params setup = {};

	if (vsm::any_flags(args.options, io_uring_options::kernel_thread))
	{
		setup.flags |= IORING_SETUP_SQPOLL;

		if (args.kernel_thread != nullptr)
		{
			setup.wq_fd = static_cast<uint32_t>(
				args.kernel_thread->m_multiplexer->m_io_uring.get());
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

	auto multiplexer = std::make_unique<_io_uring_multiplexer>(
		setup,
		vsm_move(io_uring),
		vsm_move(rings.sq_ring),
		vsm_move(rings.cq_ring),
		vsm_move(sq_data));

	vsm_try_assign(multiplexer->m_wake_event, eventfd(EFD_CLOEXEC | EFD_NONBLOCK));

	// Upon construction of the io_uring, a multishot poll operation is submitted on the wake event.
	// Any time the event enters a signaled state, the completion wakes up the poll thread.
	{
		record_context ctx(*multiplexer);
		vsm_try_discard(ctx.push(
		{
			.opcode = IORING_OP_POLL_ADD,
			.fd = multiplexer->m_wake_event.get(),
			.len = IORING_POLL_ADD_MULTI,
			.poll_events = POLLIN,
		}));
		vsm_try_void(ctx.commit());
	}

	return vsm::result<io_uring_multiplexer>(
		vsm::result_value,
		vsm_lazy(io_uring_multiplexer(vsm_move(multiplexer))));
}


vsm::result<void> _io_uring_multiplexer::attach_platform_handle(
	native_platform_handle const handle,
	connector_type& c)
{
	//TODO: Implement registered files.
	c.file_index = -1;

	return {};
}

vsm::result<void> _io_uring_multiplexer::detach_platform_handle(
	native_platform_handle const handle,
	connector_type& c)
{
	if (handle == native_platform_handle::null)
	{
		return vsm::unexpected(error::handle_cannot_be_detached);
	}

	return {};
}

vsm::result<void> _io_uring_multiplexer::commit()
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

bool _io_uring_multiplexer::is_kernel_thread_inactive() const
{
	return m_k_flags.load(std::memory_order_acquire) & IORING_SQ_NEED_WAKEUP;
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

void _io_uring_multiplexer::acquire_cqes()
{
	m_cq_produce = m_k_cq_produce.load(std::memory_order_acquire);
}

bool _io_uring_multiplexer::has_pending_cqes() const
{
	return m_cq_consume != m_cq_produce;
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

void _io_uring_multiplexer::reap_cqe(io_uring_cqe const& cqe)
{
	if (vsm::no_flags(cqe.flags, IORING_CQE_F_MORE))
	{
		// Whatever happens, after this function returns the CQE is free to be used again.
		++m_cq_free;
	}

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

	operation_type* operation = nullptr;

	if (vsm::any_flags(tag, user_data_tag::io_slot))
	{
		status.slot = static_cast<io_slot*>(user_data.ptr());

		// Emulate IOSQE_CQE_SKIP_SUCCESS for successful operations.
		if (status.result >= 0 && status.slot->m_cqe_skip_success)
		{
			return;
		}

		operation = status.slot->get_operation();
	}
	else
	{
		operation = static_cast<operation_type*>(user_data.ptr());
	}

	vsm_assert(operation->get_handler());

	// The CQE is invalidated when the handler is notified, so it is important not to pass any
	// references to the CQE to the handler.
	operation->get_handler()->notify(vsm_move(status));
}

vsm::result<void> _io_uring_multiplexer::submit_async_cancel(user_data_ptr const user_data)
{
	record_context ctx(*this);
	vsm_try_discard(ctx.push(
	{
		.opcode = IORING_OP_ASYNC_CANCEL,
		.addr = vsm::reinterpret_pointer_cast<uintptr_t>(user_data),
	}));
	return ctx.commit();
}

vsm::result<void> _io_uring_multiplexer::request_sync_cancel(user_data_ptr const user_data)
{
	io_uring_sync_cancel_reg reg =
	{
		.addr = vsm::reinterpret_pointer_cast<uintptr_t>(user_data),
	};

	return io_uring_register(
		m_io_uring.get(),
		IORING_REGISTER_SYNC_CANCEL,
		&reg,
		/* nr_args: */ 1);
}

void _io_uring_multiplexer::cancel_io(operation_type& operation, user_data_ptr const user_data)
{
	static constexpr auto cancel_requested = std::to_underlying(io_handler_tag::cancel_requested);

	// Set the cancel_requested flag, or return if it has already been set.
	if (vsm::atomic_ref const handler(operation.m_handler);
		(cancel_requested & handler.load(std::memory_order_acquire)) ||
		(cancel_requested & handler.fetch_or(cancel_requested, std::memory_order_acq_rel)))
	{
		return;
	}

	if (is_externally_synchronized())
	{
		auto const r = submit_async_cancel(user_data);

		if (!r)
		{
			//TODO: Handle errors other than SQE exhaustion?
			vsm_assert(r.error() == make_error_code(error::device_or_resource_busy));

			m_cancel_list.push_back(operation);
		}
	}
	else if (vsm::any_flags(m_flags, flags::sync_cancel))
	{
		unrecoverable(request_sync_cancel(user_data));
	}
	else if (m_cancel_mpsc_queue.push_back(operation))
	{
		if (!m_cancel_pending.load(std::memory_order_acquire) &&
			!m_cancel_pending.exchange(true, std::memory_order_acq_rel))
		{
			wake_poll_thread();
		}
	}
}

void _io_uring_multiplexer::wake_poll_thread()
{
	if (!m_wake_requested.load(std::memory_order_acquire) &&
		!m_wake_requested.exchange(true, std::memory_order_acq_rel))
	{
		// Signal the continuously polled wake event in order to wake up the poll thread if it
		// happens to be waiting on io_uring_enter.
		unrecoverable(eventfd_signal(m_wake_event.get()));
	}
}

void _io_uring_multiplexer::wake_poll_thread_reset()
{
	if (m_wake_requested.load(std::memory_order_acquire))
	{
		do
		{
			// Before resetting the atomic flag, the event object must be reset.
			unrecoverable(vsm::discard_value(eventfd_reset(m_wake_event.get())));
		}
		while (m_wake_requested.exchange(false, std::memory_order_acq_rel));
	}
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
	optional_scoped_synchronization(Externallysynchronized& object)
	{
		if (!object.is_externally_synchronized())
		{
			m_object.emplace(object);
		}
	}
};

} // namespace

vsm::result<bool> _io_uring_multiplexer::poll(deadline_t const& args)
{
	optional_scoped_synchronization const synchronization(*this);

	using reason = enter_reason;

	enter_reason enter_reason = reason::none;
	unsigned int enter_to_submit = 0;
	unsigned int enter_min_complete = 0;
	unsigned int enter_flags = 0;

	if (m_cancel_pending.load(std::memory_order_acquire))
	{
		do
		{
			// Popping the queue elements in reversed order avoids reversing the list eagerly.
			auto forward_list = m_cancel_mpsc_queue.pop_all_reversed();

			// Insert the forward list elements into the cancel list in their original order:
			while (!forward_list.empty())
			{
				auto& operation = forward_list.pop_front();
				m_operation_list.insert_before(m_cancel_list_end, operation);
			}
		}
		while (m_cancel.exchange(false, std::memory_order_acq_rel));
	}

	while (!m_operation_list.empty())
	{
		auto& operation = m_operation_list.pop_front();
		auto const handler = operation.get_handler();

		bool const made_progress = vsm::any_flags(handler.tag(), io_handler_tag::cancel_requested)
			? handler->cancel()
			: handler->submit();

		if (!made_progress)
		{
			break;
		}
	}

	// If the auto submit mode is not enabled, submit all pending SQEs now.
	if (vsm::no_flags(m_flags, flags::auto_submit) && has_pending_sqes())
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

	// Check for new CQEs produced by the kernel. If there are any, entering will not be necessary.
	// In any case, we want to enter before reaping the completions in order to prioritize
	// submissions.
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

		if (args.deadline != deadline::never())
		{
			if (args.deadline == deadline::instant())
			{
				enter_min_complete = 0;
			}
			else if (vsm::any_flags(m_flags, flags::enter_ext_arg))
			{
				// If IORING_FEAT_EXT_ARG is available, a timeout can be specified using the
				// extended io_uring_getevents_arg parameter structure.
				enter_flags |= IORING_ENTER_EXT_ARG;
				enter_timespec = make_timespec<__kernel_timespec>(args.deadline);
				enter_arg.ts = reinterpret_cast<uintptr_t>(&enter_timespec);
				p_enter_arg = &enter_arg;
			}
			else
			{
				enter_min_complete = 0;

				// Submitting a timeout operation is possible, but the timespec lifetime and
				// cancellation required make it very complicated. Instead we'll just fall back to
				// polling the io_uring object.
				vsm_try_discard(linux::poll(m_io_uring.get(), POLLIN, args.deadline));

				//TODO: Also specify POLLOUT depending on the multiplexer state?
			}
		}

		vsm_try(consumed, io_uring_enter(
			m_io_uring.get(),
			enter_to_submit,
			enter_min_complete,
			enter_flags,
			p_enter_arg));

		//TODO: Do we need to handle consumed if kernel thread is not being used?
		(void)consumed;

		// Check for new CQEs produced by the kernel.
		acquire_cqes();
	}

	// Finally, reap the pending CQEs, invoking their notify callbacks if necessary.
	return reap_all_cqes();
}
