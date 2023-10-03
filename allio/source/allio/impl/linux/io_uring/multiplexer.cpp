#include <allio/linux/detail/io_uring/multiplexer.hpp>

#include <vsm/flags.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

namespace {

static int io_uring_setup(unsigned const entries, io_uring_params* const p)
{
	return syscall(__NR_io_uring_setup, entries, p);
}

static int io_uring_enter(int const fd, unsigned const to_submit, unsigned const min_complete, unsigned const flags, void const* const arg, size_t const argsz)
{
	return syscall(__NR_io_uring_enter, fd, to_submit, min_complete, flags, arg, argsz);
}

static int io_uring_register(int const fd, unsigned const opcode, void* const arg, unsigned const nr_args)
{
	return syscall(__NR_io_uring_register, fd, opcode, arg, nr_args);
}


enum user_data_tag : uintptr_t
{
	io_slot                 = 1 << 0,

	all = io_slot
};
vsm_flag_enum(user_data_tag);

using user_data_ptr = vsm::tag_ptr<
	io_uring_multiplexer::io_data,
	user_data_tag,
	user_data_tag::all>;

} // namespace

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

vsm::result<io_uring_multiplexer> io_uring_multiplexer::_create(io_parameters_t<create_t> const& args)
{
	auto const round_up_to_power_of_two = [](uint32_t const value) -> vsm::result<uint32_t>
	{
		uint32_t const lz = std::countl_zero(value);
		if (lz == 0 && (value & value - 1) != 0)
		{
			return vsm::unexpected(error::invalid_argument);
		}
		return static_cast<uint32_t>(1) << (31 - lz);
	};

	vsm_try(min_entries, round_up_to_power_of_two(
		std::max(
			static_cast<uint32_t>(32),
			std::max(
				args.min_submission_queue_size,
				args.min_completion_queue_size))));

	io_uring_params setup = {};

	if (args.kernel_thread)
	{
		setup.flags |= IORING_SETUP_SQPOLL;

		if (*args.kernel_thread != nullptr)
		{
			setup.wq_fd = (*args.kernel_thread)->m_io_uring.get();
		}
	}

	vsm_try(io_uring, [&]() -> vsm::result<unique_fd>
	{
		int const io_uring = io_uring_setup(
			min_entries,
			&setup);

		if (io_uring == -1)
		{
			return vsm::unexpected(get_last_error());
		}

		return vsm::result<unique_fd>(vsm::result_value, io_uring);
	}());

	struct ring_pair
	{
		unique_byte_mmap sq_ring;
		unique_byte_mmap cq_ring;
	};

	vsm_try_bind((sq_ring, cq_ring), [&]() -> vsm::result<ring_pair>
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
	});

	vsm_try(sq_data, mmap<sqe_placeholder>(
		io_uring.get(),
		IORING_OFF_SQES,
		setup.sq_entries * get_sqe_size(setup)));

	return vsm_lazy(io_uring_multiplexer(
		vsm_move(io_uring),
		vsm_move(sq_ring),
		vsm_move(cq_ring),
		vsm_move(sq_data),
		setup));
}

static vsm::atomic_ref<uint32_t> get_shared_uint32(auto const& mmap, size_t const offset)
{
	return vsm::atomic_ref<uint32_t>(*reinterpret_cast<uint32_t*>(mmap.get() + offset));
}

io_uring_multiplexer::io_uring_multiplexer(
	unique_fd&& io_uring,
	unique_byte_mmap&& sq_ring,
	unique_byte_mmap&& cq_ring,
	unique_sqes_mmap&& sq_data,
	io_uring_params const& setup)
	: m_io_uring(vsm_move(io_uring))
	, m_sq_mmap(vsm_move(sq_ring))
	, m_cq_mmap(vsm_move(cq_ring))
	, m_sqes(vsm_move(sq_data))
	, m_cqes(reinterpret_cast<cqe_placeholder*>(m_cq_mmap.get() + setup.cq_off.cqes))

	, m_flags{}
	, m_sqe_size(get_sqe_size(setup))
	, m_cqe_size(get_cqe_size(setup))
	, m_sqe_multiply_shift(std::countr_zero(m_sqe_size))
	, m_cqe_multiply_shift(std::countr_zero(m_cqe_size))

	, m_k_sq_produce(get_shared_uint32(m_sq_mmap, setup.sq_off.tail))
	, m_k_sq_consume(get_shared_uint32(m_sq_mmap, setup.sq_off.head))
	, m_k_sq_array(get_shared_uint32(m_sq_mmap, setup.sq_off.array))
	, m_k_cq_produce(get_shared_uint32(m_cq_mmap, setup.cq_off.tail))
	, m_k_cq_consume(get_shared_uint32(m_cq_mmap, setup.cq_off.head))
	, m_k_flags(get_shared_uint32(m_sq_mmap, setup.sq_off.flags))

	, m_sq_size(setup.sq_entries)
	, m_sq_free(m_sq_size)
	, m_sq_acquire(m_k_sq_consume.load(std::memory_order_relaxed))
	, m_sq_release(m_sq_acquire)

	, m_cq_size(setup.cq_entries)
	, m_cq_free(m_cq_size)
	, m_cq_consume(m_k_cq_produce.load(std::memory_order_relaxed))
{
	if (setup.features & IORING_FEAT_EXT_ARG)
	{
		m_flags |= flags::enter_ext_arg;
	}

	if (setup.features & IORING_SETUP_SQPOLL)
	{
		m_flags |= flags::kernel_thread;
	}

	for (size_t i = 0; i < m_sq_size; ++i)
	{
		m_k_sq_array[m_sq_acquire + i & m_sq_size - 1] = i;
	}
}


vsm::result<void> io_uring_multiplexer::_attach_handle(_platform_handle const& h, connector_type& c)
{
	return {};
}

void io_uring_multiplexer::_detach_handle(_platform_handle const& h, connector_type& c)
{
}


vsm::result<void> io_uring_multiplexer::record_context::push_linked_timeout(timeout::reference const timeout)
{
#if __cpp_lib_is_layout_compatible
	static_assert(std::is_layout_compatible_v<timeout::timespec, __kernel_timespec>);
#else
	static_assert(sizeof(timeout::timespec) == sizeof(__kernel_timespec));
	static_assert(alignof(timeout::timespec) == alignof(__kernel_timespec));
#endif

	//TODO: Make sure linked timeouts never produce a CQE.

	if (m_sq_acquire == m_multiplexer.m_sq_consume)
	{
		return vsm::unexpected(error::too_many_concurrent_async_operations);
	}

	io_uring_sqe& sqe = m_multiplexer.get_sqe(m_multiplexer.get_sqe_offset(m_sq_acquire++));

	sqe.opcode = IORING_OP_LINK_TIMEOUT;
	sqe.flags = IOSQE_IO_LINK;
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
	if (m_cq_free == 0)
	{
		return vsm::unexpected(error::too_many_concurrent_async_operations);
	}

	if (m_sq_acquire == m_multiplexer.m_sq_consume)
	{
		return vsm::unexpected(error::too_many_concurrent_async_operations);
	}

	--m_cq_free;
	io_uring_sqe& sqe = m_multiplexer.get_sqe(m_multiplexer.get_sqe_offset(m_sq_acquire++));
	sqe.user_data = reinterpret_pointer_cast<uintptr_t>(data);

	return &sqe;
}

vsm::result<void> io_uring_multiplexer::record_context::commit()
{
	m_multiplexer.m_sq_acquire = m_sq_acquire;
	m_multiplexer.m_cq_free = m_cq_free;
	return m_multiplexer.commit();
}


vsm::result<void> io_uring_multiplexer::commit()
{
	if (vsm::any_flags(m_flags, flags::auto_submit))
	{
		uint32_t const sq_offset = m_sq_acquire;
		m_sq_release = sq_offset;

		// Make the produced SQEs visible to the kernel.
		m_k_sq_produce.store(sq_offset, std::memory_order_release);

		if (vsm::any_flags(m_flags, flags::kernel_thread))
		{
			wake_kernel_thread();
		}
		else
		{
			//TODO: enter
		}
	}

	return {};
}


bool io_uring_multiplexer::kernel_thread_needs_wakeup()
{
	return vsm::any_flags(
		m_k_flags.load(std::memory_order_acquire),
		IORING_SQ_NEED_WAKEUP);
}

void io_uring_multiplexer::wake_kernel_thread()
{
	vsm_assert(vsm::any_flags(m_flags, flags::kernel_thread)); //PRECONDITION

	if (kernel_thread_needs_wakeup())
	{
		int const r = io_uring_enter(
			m_io_uring.get(),
			/* to_submit: */ 0,
			/* min_complete: */ 0,
			IORING_ENTER_SQ_WAKEUP,
			/* arg: */ nullptr,
			/* argsz: */ 0);

		unrecoverable_error(static_cast<system_error>(r));
	}
}


uint32_t io_uring_multiplexer::_reap_completion_queue()
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

			vsm_assert(user_data.ptr() != nullptr);
			user_data_tag const tag = user_data.tag();

			bool const has_slot = vsm::any_flags(tag, user_data_tag::io_slot);
			operation_type& operation = has_slot
				? *static_cast<io_slot*>(user_data.ptr())->m_operation
				: *static_cast<operation_type*>(user_data.ptr());

			io_status_type const status =
			{
				.slot = has_slot
					? static_cast<io_slot*>(user_data.ptr())
					: nullptr,
				.result = cqe.res,
				.flags = cqe.flags,
			};
			operation.notify(io_status(status));
		}

		m_cq_k_consume.store(cq_produce, std::memory_order_release);
		m_cq_consume = cq_produce;
	}

	return cq_produce - cq_consume;
}

vsm::result<bool> io_uring_multiplexer::_poll(io_parameters_t<poll_t> const& args)
{
	if (vsm::any_flags(args.mode, pump_mode::submit))
	{
		//TODO: Submit
	}

	if (vsm::any_flags(args.moce, pump_mode::complete))
	{
		_flush_completion_queue();
	}

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

		}
	}
}
