#pragma once

#include <allio/detail/deadline.hpp>
#include <allio/step_deadline.hpp>
#include <allio/linux/detail/io_uring/io_uring.hpp>
#include <allio/linux/detail/io_uring/multiplexer.hpp>

#include <allio/linux/detail/undef.i>

namespace allio {

class _io_uring_multiplexer::record_context
{
	static constexpr uint8_t link_flags = IOSQE_IO_LINK | IOSQE_IO_HARDLINK;

	_io_uring_multiplexer& m_multiplexer;
	step_deadline m_step_deadline;

	uint32_t m_sq_acquire;
	io_uring_sqe* m_last_sqe;

public:
	explicit record_context(
		_io_uring_multiplexer& multiplexer,
		deadline const deadline = deadline::never())
		: m_multiplexer(multiplexer)
		, m_step_deadline(deadline)
		, m_sq_acquire(multiplexer.m_sq_acquire)
		, m_last_sqe(nullptr)
	{
		m_multiplexer.enter_record_context();
	}

	explicit record_context(
		io_uring_multiplexer& multiplexer,
		deadline const deadline = deadline::never())
		: record_context(*multiplexer.m_multiplexer, deadline)
	{
	}

	record_context(record_context const&) = delete;
	record_context& operator=(record_context const&) = delete;

	~record_context()
	{
		m_multiplexer.leave_record_context();
	}


	[[nodiscard]] io_result<io_uring_sqe*> acquire_sqe()
	{
		if (m_sq_acquire - m_multiplexer.m_sq_consume == m_multiplexer.m_sq_size)
		{
			vsm_try(deadline, m_step_deadline.step());
			vsm_try_void(m_multiplexer.wait_for_sqe(deadline));
		}

		vsm_assert(m_sq_acquire - m_multiplexer.m_sq_consume != m_multiplexer.m_sq_size);
		return m_last_sqe = &m_multiplexer.get_sqes()[m_sq_acquire++];
	}


	struct fd_pair
	{
		int32_t fd;
		uint8_t fd_flags;
	};

	[[nodiscard]] fd_pair get_fd(connector_type const& c, native_platform_handle const handle) const
	{
		return c.file_index != -1
			? fd_pair{ c.file_index, IOSQE_FIXED_FILE }
			: fd_pair{ unwrap_handle(handle), 0 };
	}


	[[nodiscard]] uintptr_t get_user_data(operation_type& operation) const
	{
		return vsm::reinterpret_pointer_cast<uintptr_t>(
			basic_user_data_ptr<operation_type>(&operation));
	}

	[[nodiscard]] uintptr_t get_user_data(io_slot& slot) const
	{
		return vsm::reinterpret_pointer_cast<uintptr_t>(
			basic_user_data_ptr<io_slot>(&slot, user_data_tag::io_slot));
	}


	[[nodiscard]] io_result<io_uring_sqe*> push()
	{
		vsm_try(p_sqe, acquire_sqe());
		m_last_sqe = p_sqe;
		return p_sqe;
	}

	void link_last(uint8_t const flags) const
	{
		vsm_assert(m_last_sqe != nullptr); //PRECONDITION

		vsm_assert((flags & link_flags) != 0); //PRECONDITION
		vsm_assert((flags & ~link_flags) == 0); //PRECONDITION

		m_last_sqe->flags |= flags;
	}


	[[nodiscard]] vsm::result<io_uring_sqe*> push_timeout(timeout::reference const timeout)
	{
		vsm_try(p_sqe, push());
		*p_sqe = io_uring_sqe
		{
			.opcode = IORING_OP_TIMEOUT,
			.addr = reinterpret_cast<uintptr_t>(&timeout.m_timespec),
			.len = 1,
			.timeout_flags = timeout.m_absolute ? IORING_TIMEOUT_ABS : 0,
		};
		return p_sqe;
	}

	[[nodiscard]] vsm::result<void> link_timeout(timeout::reference const timeout)
	{
		vsm_assert(m_last_sqe != nullptr); //PRECONDITION
		vsm_assert(m_last_sqe->opcode != IORING_OP_LINK_TIMEOUT); //PRECONDITION

		vsm_try(p_sqe, acquire_sqe());

		*p_sqe = io_uring_sqe
		{
			.opcode = IORING_OP_LINK_TIMEOUT,
			.flags = m_multiplexer.m_cqe_skip_success_flag,
			.addr = reinterpret_cast<uintptr_t>(&timeout.m_timespec),
			.len = 1,
			.timeout_flags = timeout.m_absolute ? IORING_TIMEOUT_ABS : 0,
		};

		m_last_sqe->flags |= IOSQE_IO_LINK;
		m_last_sqe = p_sqe;

		return {};
	}


	/// @brief Set IOSQE_CQE_SKIP_SUCCESS on @param sqe if it is available.
	void set_cqe_skip_success(io_uring_sqe& sqe)
	{
		sqe.flags |= m_multiplexer.m_cqe_skip_success_flag;
	}

	/// @brief Emulate skipping of CQEs of operations on success. Any CQEs associated with this slot
	///        are skipped if successful.
	void set_cqe_skip_success_emulation(io_slot& slot)
	{
		slot.m_flags |= io_slot_flags::cqe_skip_success;
	}

	/// @brief Emulate skipping of CQEs of linked operations on failure. The CQE associated with
	///        this SQE is skipped if the result is ECANCELED. The primary purpose of skipping the
	///        CQE is to avoid extending the lifetime of the associated io_slot until all linked
	///        operations are canceled.
	/// @pre @param sqe has associated user data. CQEs without user data are always skipped.
	/// @note This prevents the direct manual cancellation of the affected operation.
	void set_cqe_skip_success_linked_emulation(io_uring_sqe& sqe)
	{
		vsm_assert(sqe.user_data != 0); //PRECONDITION
		sqe.user_data |= static_cast<uintptr_t>(user_data_tag::cqe_skip_cancel);
	}


	void commit()
	{
		vsm_assert(m_last_sqe != nullptr); //PRECONDITION

		vsm_assert(
			(m_last_sqe->flags & link_flags) == 0 &&
			"The final SQE in a series may not be linked.");

		vsm_assert(
			(m_last_sqe->flags & IOSQE_CQE_SKIP_SUCCESS) == 0 &&
			"The CQE of the final SQE in a series may not be skipped.");

		m_multiplexer.m_sq_acquire = m_sq_acquire;

		m_last_sqe = nullptr;

		//TODO: Should this attempt kernel thread wake up?
	}
};

using io_uring_record_context = _io_uring_multiplexer::record_context;

} // namespace

#include <allio/linux/detail/undef.i>
