#pragma once

#include <allio/linux/detail/io_uring/multiplexer.hpp>

#include <allio/linux/platform.hpp>

#include <vsm/lazy.hpp>

#include <linux/io_uring.h>

#include <allio/linux/detail/undef.i>

class allio::detail::io_uring_multiplexer::record_context
{
	static constexpr uint8_t link_flags = IOSQE_IO_LINK | IOSQE_IO_HARDLINK;


	io_uring_multiplexer& m_multiplexer;

	uint32_t m_sq_acquire;
	uint32_t m_cq_free;

	uint32_t m_cq_require;

	io_uring_sqe* m_last_sqe = nullptr;

public:
	explicit record_context(io_uring_multiplexer& multiplexer)
		: m_multiplexer(multiplexer)
		, m_sq_acquire(multiplexer.m_sq_acquire)
		, m_cq_free(multiplexer.m_cq_free)
	{
		vsm_assert(m_multiplexer.acquire_record_lock() &&
			"The I/O recording context may not be re-entered.");
	}

	record_context(record_context const&) = delete;
	record_context& operator=(record_context const&) = delete;

	~record_context()
	{
		vsm_assert(m_multiplexer.release_record_lock() &&
			"The I/O recording context may not be re-entered.");
	}


	struct fd_pair
	{
		int32_t fd;
		uint8_t fd_flags;
	};

	fd_pair get_fd(connector_type const& c, native_platform_handle const handle) const
	{
		return c.file_index != -1
			? fd_pair{ c.file_index, IOSQE_FIXED_FILE }
			: fd_pair{ linux::unwrap_handle(handle), 0 };
	}


	uintptr_t get_user_data(operation_type& operation) const
	{
		return reinterpret_cast<uintptr_t>(&operation);
	}

	uintptr_t get_user_data(io_slot& io_slot) const
	{
		return vsm::reinterpret_pointer_cast<uintptr_t>(user_data_ptr(
			&io_slot,
			user_data_tag::io_slot));
	}


	vsm::result<io_uring_sqe*> push()
	{
		vsm_try(p_sqe, acquire_sqe(/* acquire_cqe: */ true));
		m_last_sqe = p_sqe;
		return p_sqe;
	}

	template<std::convertible_to<io_uring_sqe> SQE = io_uring_sqe>
	vsm::result<io_uring_sqe*> push(SQE&& sqe)
	{
		vsm_try(p_sqe, push());
		*p_sqe = vsm_forward(sqe);
		return p_sqe;
	}

	void link_last(uint8_t const flags) const
	{
		vsm_assert(m_last_sqe != nullptr); //PRECONDITION

		vsm_assert((flags & link_flags) != 0); //PRECONDITION
		vsm_assert((flags & ~link_flags) == 0); //PRECONDITION

		m_last_sqe->flags |= flags;
	}


	vsm::result<io_uring_sqe*> push_timeout(timeout::reference const timeout)
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

	vsm::result<void> link_timeout(timeout::reference const timeout)
	{
		vsm_assert(m_last_sqe != nullptr); //PRECONDITION
		vsm_assert(m_last_sqe->opcode != IORING_OP_LINK_TIMEOUT); //PRECONDITION

		// IORING_OP_LINK_TIMEOUT with IOSQE_CQE_SKIP_SUCCESS never posts a CQE.
		vsm_try(p_sqe, acquire_sqe(/* acquire_cqe: */ m_multiplexer.m_cqe_skip_success == 0));

		*p_sqe = io_uring_sqe
		{
			.opcode = IORING_OP_LINK_TIMEOUT,
			.flags = m_multiplexer.m_cqe_skip_success,
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
		sqe.flags |= m_multiplexer.m_cqe_skip_success;
	}

	/// @brief Emulate skipping of CQEs of operations on success.
	///        Any CQEs associated with this slot are skipped if successful.
	void set_cqe_skip_success_emulation(io_slot& slot)
	{
		slot.m_operation.set_tag(slot.m_operation.tag() | operation_tag::cqe_skip_success);
	}

	/// @brief Emulate skipping of CQEs of linked operations on failure.
	///        The CQE associated with this SQE is skipped if the result is ECANCELED.
	///        The primary purpose of skipping the CQE is to avoid extending the lifetime
	///        of the associated io_data until all linked operations are canceled.
	/// @pre @param sqe has associated user data. CQEs without user data are always skipped.
	/// @note This prevents the direct manual cancellation of the affected operation.
	void set_cqe_skip_success_linked_emulation(io_uring_sqe& sqe)
	{
		vsm_assert(sqe.user_data != 0); //PRECONDITION
		sqe.user_data |= static_cast<uintptr_t>(user_data_tag::cqe_skip_cancel);
	}


	vsm::result<void> commit()
	{
		vsm_assert(m_last_sqe != nullptr); //PRECONDITION

		vsm_assert((m_last_sqe->flags & link_flags) == 0 &&
			"The final SQE in a series may not be linked.");

		vsm_assert((m_last_sqe->flags & IOSQE_CQE_SKIP_SUCCESS) == 0 &&
			"The CQE of the final SQE in a series may not be skipped.");

		m_multiplexer.m_sq_acquire = m_sq_acquire;
		m_multiplexer.m_cq_free = m_cq_free;
		m_last_sqe = nullptr;

		return m_multiplexer.commit();
	}

private:
	vsm::result<io_uring_sqe*> acquire_sqe(bool const acquire_cqe)
	{
		if (m_sq_acquire - m_multiplexer.m_sq_consume == m_multiplexer.m_sq_size)
		{
			return vsm::unexpected(error::too_many_concurrent_async_operations);
		}

		if (acquire_cqe)
		{
			if (m_cq_free == 0)
			{
				return vsm::unexpected(error::too_many_concurrent_async_operations);
			}

			--m_cq_free;
		}

		return m_last_sqe = &m_multiplexer.get_sqes()[m_sq_acquire++];
	}
};

#include <allio/linux/detail/undef.i>
