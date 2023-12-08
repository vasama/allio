#pragma once

#include <allio/detail/external_synchronization.hpp>
#include <allio/detail/handles/platform_object.hpp>
#include <allio/detail/io.hpp>
#include <allio/detail/multiplexer.hpp>
#include <allio/linux/detail/unique_fd.hpp>
#include <allio/linux/detail/unique_mmap.hpp>
#include <allio/linux/timespec.hpp>

#include <vsm/atomic.hpp>
#include <vsm/flags.hpp>
#include <vsm/result.hpp>
#include <vsm/tag_ptr.hpp>

#include <allio/linux/detail/undef.i>

struct io_uring_params;
struct io_uring_sqe;
struct io_uring_cqe;

namespace allio::detail {

class io_uring_multiplexer;

namespace io_uring {

/// @brief Detect kernel support for io_uring.
/// @return Returns true if the kernel provides some level of support for io_uring.
bool is_supported();

struct kernel_thread_t
{
	std::optional<io_uring_multiplexer const*> kernel_thread;
};
inline constexpr explicit_ref_parameter<kernel_thread_t, io_uring_multiplexer const> kernel_thread = {};

struct submission_queue_size_t
{
	size_t submission_queue_size = 0;
};
inline constexpr explicit_parameter<submission_queue_size_t> submission_queue_size = {};

struct completion_queue_size_t
{
	size_t completion_queue_size = 0;
};
inline constexpr explicit_parameter<completion_queue_size_t> completion_queue_size = {};

} // namespace io_uring

class io_uring_multiplexer final : public externally_synchronized
{
	/// Submission ring buffer.
	///
	/// Each index is a position in the submission ring buffer.
	/// The indices prefixed with "k" are shared with the kernel.
	///
	/// Each index is limited to less than or equal to the next.
	/// Wraparound is possible, but that does not matter.
	/// Imagine instead that the indices have infinite precision.
	///
	/// @verbatim
	/// |
	/// | <- @ref m_k_sq_consume
	/// |    One after the entry most recently read by the kernel.
	/// |    That slot can now be reused for another entry.
	/// |
	/// | <- @ref m_k_sq_produce
	/// |    One after the entry most recently submitted to the kernel.
	/// |
	/// | <- @ref m_sq_release
	/// |    One after the entry most recently recorded, and which is now ready for submission.
	/// |
	/// | <- @ref m_sq_acquire
	/// |    Free slot where a new entry can be recorded.
	/// |
	/// | <- @ref m_sq_consume
	/// |    Cached copy of m_k_sq_consume.
	/// |
	/// | <- @ref m_k_sq_consume
	/// |    The ring buffer wraps around.
	/// V
	/// @endverbatim

	/// Completion queue ring buffer
	///
	/// @verbatim
	/// |
	/// V
	/// @endverbatim

	enum class flags : uint32_t
	{
		enter_ext_arg                   = 1 << 0,
		kernel_thread                   = 1 << 1,
		cqe_skip_success                = 1 << 2,
		auto_submit                     = 1 << 3,
		record_lock                     = 1 << 4,
	};
	vsm_flag_enum_friend(flags);

	enum class user_data_tag : uintptr_t
	{
		io_slot                     = 1 << 0,
		cqe_skip_cancel             = 1 << 1,

		all = io_slot | cqe_skip_cancel
	};
	vsm_flag_enum_friend(user_data_tag);

	template<typename T>
	using basic_user_data_ptr = vsm::tag_ptr<T, user_data_tag, user_data_tag::all>;

	using user_data_ptr = vsm::incomplete_tag_ptr<void, user_data_tag, user_data_tag::all>;

public:
	using multiplexer_concept = void;

	struct io_status_type;

	class connector_type : public async_connector_base
	{
		int32_t file_index = -1;

		friend io_uring_multiplexer;
	};

	class operation_type : public async_operation_base
	{
		friend io_uring_multiplexer;
	};

private:
	using io_handler_type = basic_io_handler<io_status_type>;

	enum class handler_tag : uintptr_t
	{
		cqe_skip_success = 1 << 0,

		all = cqe_skip_success
	};
	vsm_flag_enum_friend(handler_tag);

	using handler_ptr = vsm::tag_ptr<io_handler_type, handler_tag, handler_tag::all>;

public:
	class io_slot
	{
		handler_ptr m_handler = nullptr;

	public:
		void bind(io_handler_type& handler) &
		{
			m_handler = &handler;
		}

		friend io_uring_multiplexer;
	};

	struct io_status_type
	{
		io_slot* slot;
		int32_t result;
		uint32_t flags;
	};

private:
	/// @brief Unique owner of the io uring kernel object.
	unique_fd m_io_uring;

	/// @brief Unique owner of the mmapped region containing the submission queue indices.
	unique_byte_mmap m_sq_mmap;

	/// @brief Unique owner of the mmapped region containing the completion queue entries.
	unique_byte_mmap m_cq_mmap;

	/// @brief Ring of submission queue entries.
	/// @note Written by user space, read by the kernel.
	unique_void_mmap m_sqes;

	/// @brief Ring of completion queue entries.
	/// @note Written by the kernel, read by user space.
	void* m_cqes;


	flags m_flags;

	/// @brief Dynamic size of a single @ref io_uring_sqe.
	uint8_t m_sqe_size;

	/// @brief Dynamic size of a single @ref io_uring_cqe.
	uint8_t m_cqe_size;

	/// @brief Shift used for multiplication by @ref m_sqe_size.
	uint8_t m_sqe_multiply_shift;

	/// @brief Shift used for multiplication by @ref m_cqe_size.
	uint8_t m_cqe_multiply_shift;

	uint8_t m_cqe_skip_success;


	/// @brief One past the newest submission queue entry produced by user space.
	/// @note Written by user space, read by the kernel.
	vsm::atomic_ref<uint32_t> m_k_sq_produce;

	/// @brief One past the newest submission queue entry consumed by the kernel.
	/// @note The value always trails @ref m_k_sq_produce.
	/// @note Written by the kernel, read by user space.
	vsm::atomic_ref<uint32_t> m_k_sq_consume;

	/// @brief Maps logical SQE index to physical index in @ref m_sqes.
	/// @note Written by user space, read by user space and the kernel.
	uint32_t* m_k_sq_array;

	/// @brief One past the newest completion queue entry produced by the kernel.
	/// @note Written by the kernel, read by user space.
	vsm::atomic_ref<uint32_t> m_k_cq_produce;

	/// @brief One past the newest completion queue entry consumed by user space.
	/// @note The value always trails @ref m_k_cq_produce.
	/// @note Written by user space, read by the kernel.
	vsm::atomic_ref<uint32_t> m_k_cq_consume;

	/// @brief Describes the io uring dynamic state.
	/// @note Written by the kernel, read by user space.
	vsm::atomic_ref<uint32_t> m_k_flags;


	/// @brief Size of the submission queue buffer.
	uint32_t m_sq_size;

	/// @brief Number of currently free SQEs.
	uint32_t m_sq_free;

	uint32_t m_sq_consume;

	/// @brief One past the SQE most recently 
	/// @note @ref m_sq_ready <= m_sq_acquire <= @ref m_sq_consume
	uint32_t m_sq_acquire;

	/// @brief One past the SQE most recently submitted to the kernel.
	/// @note @ref m_k_sq_produce == m_sq_release <= @ref m_sq_ready
	uint32_t m_sq_release;


	/// @brief Size of the completion queue buffer.
	uint32_t m_cq_size;

	/// @brief Number of currently free CQEs.
	uint32_t m_cq_free;

	uint32_t m_cq_produce;

	/// @brief One past the CQE most recently consumed by user space.
	/// @ref m_k_cq_consume == m_cq_consume <= @ref m_k_cq_produce
	uint32_t m_cq_consume;


	using create_parameters = parameters_t
	<
		io_uring::kernel_thread_t,
		io_uring::submission_queue_size_t,
		io_uring::completion_queue_size_t
	>;

	using poll_parameters = deadline_t;

public:
	[[nodiscard]] static vsm::result<io_uring_multiplexer> create(auto&&... args)
	{
		return _create(make_args<create_parameters>(vsm_forward(args)...));
	}


	/// @return True if the multiplexer made any progress.
	[[nodiscard]] vsm::result<bool> poll(auto&&... args)
	{
		return _poll(make_args<poll_parameters>(vsm_forward(args)...));
	}


	[[nodiscard]] vsm::result<void> attach_handle(native_platform_handle handle, connector_type& c);
	[[nodiscard]] vsm::result<void> detach_handle(native_platform_handle handle, connector_type& c);


	//void cancel_io(io_handler_type& handler);
	void cancel_io(io_slot& slot);


	class timeout
	{
		struct timespec
		{
			long long tv_sec;
			long long tv_nsec;
		};

		timespec m_timespec;

	public:
		class reference
		{
			timespec const& m_timespec;
			bool m_absolute;

			explicit reference(timespec const& timespec, bool const absolute)
				: m_timespec(timespec)
				, m_absolute(absolute)
			{
			}

			friend class io_uring_multiplexer;
		};

		reference set(deadline const deadline) &
		{
			m_timespec = make_timespec<timespec>(deadline);
			return reference(m_timespec, deadline.is_absolute());
		}

	private:
		friend class io_uring_multiplexer;
	};

	class record_context;

private:
	explicit io_uring_multiplexer(
		unique_fd&& io_uring,
		unique_byte_mmap&& sq_ring,
		unique_byte_mmap&& cq_ring,
		unique_void_mmap&& sq_data,
		io_uring_params const& setup);


	[[nodiscard]] static vsm::result<io_uring_multiplexer> _create(create_parameters const& args);


	friend vsm::result<void> tag_invoke(
		attach_handle_t,
		io_uring_multiplexer& m,
		platform_object_t::native_type const& h,
		connector_type& c)
	{
		return m.attach_handle(h.platform_handle, c);
	}

	friend vsm::result<void> tag_invoke(
		detach_handle_t,
		io_uring_multiplexer& m,
		platform_object_t::native_type const& h,
		connector_type& c)
	{
		return m.detach_handle(h.platform_handle, c);
	}


	[[nodiscard]] bool acquire_record_lock()
	{
		return !vsm::any_flags(
			std::exchange(m_flags, m_flags | flags::record_lock),
			flags::record_lock);
	}

	[[nodiscard]] bool release_record_lock()
	{
		return vsm::any_flags(
			std::exchange(m_flags, m_flags & ~flags::record_lock),
			flags::record_lock);
	}


	template<typename T>
	class ring_view
	{
		std::byte* m_ring;
		uint32_t m_mask;
		uint32_t m_multiply_shift;

	public:
		explicit ring_view(void* const ring, uint32_t const mask, uint32_t const multiply_shift)
			: m_ring(static_cast<std::byte*>(ring))
			, m_mask(mask)
			, m_multiply_shift(multiply_shift)
		{
		}

		[[nodiscard]] T& operator[](uint32_t const offset) const
		{
			return *reinterpret_cast<T*>(m_ring + ((offset & m_mask) << m_multiply_shift));
		}
	};

	[[nodiscard]] ring_view<io_uring_sqe> get_sqes()
	{
		return ring_view<io_uring_sqe>(m_sqes.get(), m_sq_size - 1, m_sqe_multiply_shift);
	}

	[[nodiscard]] ring_view<io_uring_cqe> get_cqes()
	{
		return ring_view<io_uring_cqe>(m_cqes, m_cq_size - 1, m_cqe_multiply_shift);
	}

	[[nodiscard]] vsm::result<void> commit();

	[[nodiscard]] vsm::result<void> async_cancel_io(io_slot& slot);


	[[nodiscard]] bool has_kernel_thread() const
	{
		return vsm::any_flags(m_flags, flags::kernel_thread);
	}

	[[nodiscard]] bool is_kernel_thread_inactive() const;


	[[nodiscard]] bool has_pending_sqes() const;
	void release_sqes();

	[[nodiscard]] bool has_pending_cqes() const;
	void acquire_cqes();

	[[nodiscard]] bool reap_all_cqes();
	void reap_cqe(io_uring_cqe const& cqe);


	[[nodiscard]] vsm::result<bool> _poll(poll_parameters const& args);

	friend vsm::result<bool> tag_invoke(poll_io_t, io_uring_multiplexer& m, auto&&... args)
	{
		return m._poll(make_args<poll_parameters>(vsm_forward(args)...));
	}
};

static_assert(std::is_default_constructible_v<io_uring_multiplexer::io_slot>);
static_assert(std::is_default_constructible_v<io_uring_multiplexer::timeout>);

} // namespace allio::detail

#include <allio/linux/detail/undef.i>
