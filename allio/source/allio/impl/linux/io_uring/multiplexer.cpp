#include <allio/linux/detail/io_uring/multiplexer.hpp>

#include <vsm/defer.hpp>
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

bool io_uring_multiplexer::is_supported()
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

vsm::result<io_uring_multiplexer> io_uring_multiplexer::_create(create_parameters const& args)
{

}


vsm::result<void> io_uring_multiplexer::_attach_handle(_platform_handle const& h, connector_type& c)
{
	return {};
}

void io_uring_multiplexer::_detach_handle(_platform_handle const& h, connector_type& c)
{
}


uint32_t io_uring_multiplexer::_reap_completion_queue()
{
	uint32_t const cq_consume = m_cq_consume;

	// One after the last completion queue entry produced by the kernel.
	uint32_t const cq_produce = m_cq_k_produce.load(std::memory_order_acquire);

	if (cq_produce != m_cq_consume)
	{
		uint32_t const cq_mask = m_cq_size - 1;

		uint32_t cq_offset = cq_consume;

		vsm_defer // Store cq_offset at the end of this scope.
		{
			m_cq_k_consume.store(cq_offset, std::memory_order_release);
			m_cq_consume = cq_offset;
		};

		for (; cq_offset != cq_produce; ++cq_offset)
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
	}

	return cq_produce - cq_consume;
}

vsm::result<bool> io_uring_multiplexer::_poll(poll_parameters const& args)
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
