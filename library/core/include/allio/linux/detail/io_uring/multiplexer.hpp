#pragma once

#include <allio/detail/external_synchronization.hpp>
#include <allio/detail/handles/platform_object.hpp>
#include <allio/detail/io.hpp>
#include <allio/detail/multiplexer.hpp>
#include <allio/detail/unique_handle.hpp>
#include <allio/linux/detail/unique_mmap.hpp>
#include <allio/linux/timespec.hpp>

#include <vsm/atomic.hpp>
#include <vsm/flags.hpp>
#include <vsm/intrusive/mpsc_queue.hpp>
#include <vsm/result.hpp>
#include <vsm/tag_ptr.hpp>

#include <allio/linux/detail/undef.i>

struct io_uring_params;
struct io_uring_sqe;
struct io_uring_cqe;

namespace allio::detail {

class io_uring_multiplexer;

enum class io_uring_options : uint8_t
{
	kernel_thread                       = 1 << 0,
	register_ring                       = 1 << 1,
};
vsm_flag_enum(io_uring_options);

namespace io_uring {

/// @brief Detect kernel support for io_uring.
/// @return Returns true if the kernel provides some level of support for io_uring.
bool is_supported();

struct kernel_thread_t : explicit_argument<kernel_thread_t, io_uring_multiplexer const*> {};
inline constexpr explicit_reference_parameter<kernel_thread_t> kernel_thread = {};

struct submission_queue_size_t : explicit_argument<submission_queue_size_t, uint32_t> {};
inline constexpr explicit_parameter<submission_queue_size_t> submission_queue_size = {};

struct completion_queue_size_t : explicit_argument<completion_queue_size_t, uint32_t> {};
inline constexpr explicit_parameter<completion_queue_size_t> completion_queue_size = {};

} // namespace io_uring

struct alignas(4) io_uring_slot {};

struct _io_uring_multiplexer : io_uring_slot, externally_synchronized
{
	using poll_parameters = deadline_t;

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

	enum class user_data_tag : uintptr_t
	{
		io_slot                         = 1 << 0,
		cqe_skip_cancel                 = 1 << 1,

		all = io_slot | cqe_skip_cancel
	};
	vsm_flag_enum_friend(user_data_tag);

	enum class io_handler_tag : uintptr_t
	{
		not_cancelled                   = 0,
		cancel_submitted                = 1,
		cancel_flushing                 = 2,
		cancel_pending                  = 3,
	};
	vsm_flag_enum_friend(io_handler_tag);

	enum class io_slot_flags : uint16_t
	{
		cqe_skip_success                = 1 << 0,
	};
	vsm_flag_enum_friend(io_slot_flags);

	template<typename T>
	using basic_user_data_ptr = vsm::tag_ptr<T, user_data_tag, user_data_tag::all>;

	using user_data_ptr = vsm::incomplete_tag_ptr<void, user_data_tag, user_data_tag::all>;

	struct io_status_type;
	using io_handler_type = basic_io_handler<io_status_type>;
	using io_handler_ptr = vsm::tag_ptr<io_handler_type, io_handler_tag>;

	class connector_type
	{
		int32_t file_index = -1;

		friend _io_uring_multiplexer;
	};

	class operation_type : vsm::intrusive::mpsc_queue_link
	{
		mutable uintptr_t m_handler = 0;

	public:
		void set_handler(io_handler_type& handler) &
		{
			m_handler = vsm::reinterpret_pointer_cast<uintptr_t>(io_handler_ptr(&handler));
		}

		[[nodiscard]] bool is_cancel_requested() const
		{
			return load(std::memory_order_acquire).tag() != io_handler_tag::not_cancelled;
		}

	private:
		[[nodiscard]] io_handler_ptr load(std::memory_order const memory_order) const
		{
			return vsm::reinterpret_pointer_cast<io_handler_ptr>(
				vsm::atomic_ref(m_handler).load(memory_order));
		}

		void store(io_handler_ptr const handler, std::memory_order const memory_order)
		{
			vsm::atomic_ref(m_handler).store(
				vsm::reinterpret_pointer_cast<uintptr_t>(handler),
				memory_order);
		}

		[[nodiscard]] io_handler_ptr fetch_or(
			io_handler_tag const tag,
			std::memory_order const memory_order)
		{
			return vsm::reinterpret_pointer_cast<io_handler_ptr>(
				vsm::atomic_ref(m_handler).fetch_or(static_cast<uintptr_t>(tag), memory_order));
		}

		[[nodiscard]] bool compare_exchange_strong(
			io_handler_ptr& expected_handler,
			io_handler_ptr const desired_handler,
			std::memory_order const success_memory_order,
			std::memory_order const failure_memory_order)
		{
			uintptr_t expected_value = vsm::reinterpret_pointer_cast<uintptr_t>(expected_handler);

			bool const result = vsm::atomic_ref(m_handler).compare_exchange_strong(
				expected_value,
				vsm::reinterpret_pointer_cast<uintptr_t>(desired_handler),
				success_memory_order,
				failure_memory_order);

			expected_handler = vsm::reinterpret_pointer_cast<io_handler_ptr>(expected_value);

			return result;
		}

		friend _io_uring_multiplexer;
		friend vsm::intrusive::access;
	};

	class io_slot : public io_uring_slot
	{
		uint16_t m_offset = 0;
		io_slot_flags m_flags = {};

	public:
		void bind(operation_type& operation) &
		{
			uintptr_t const uint_main = reinterpret_cast<uintptr_t>(&operation);
			uintptr_t const uint_this = reinterpret_cast<uintptr_t>(this);

			vsm_assert(uint_this > uint_main);
			vsm_assert(uint_this - uint_main <= static_cast<uint16_t>(-1));

			m_offset = static_cast<uint16_t>(uint_this - uint_main);
		}

	private:
		operation_type* get_operation() const
		{
			vsm_assert(m_offset != 0);
			uintptr_t const uint_this = reinterpret_cast<uintptr_t>(this);
			uintptr_t const uint_main = uint_this - m_offset;
			return reinterpret_cast<operation_type*>(uint_main);
		}

		friend _io_uring_multiplexer;
	};

	struct io_status_type
	{
		io_uring_slot const* slot;
		int32_t result;
		uint32_t flags;
	};


	/// @brief Unique owner of the io_uring kernel object.
	unique_handle const m_io_uring;

	int m_registered_io_uring = -1;

	/// @brief Unique owner of an eventfd used to wake the polling thread.
	unique_handle m_wake_event;

	/// @brief Unique owner of the mmapped region containing the submission queue indices.
	unique_byte_mmap const m_sq_mmap;

	/// @brief Unique owner of the mmapped region containing the completion queue entries.
	unique_byte_mmap const m_cq_mmap;

	/// @brief Ring of submission queue entries.
	/// @note Written by user space, read by the kernel.
	unique_void_mmap const m_sqes;

	/// @brief Ring of completion queue entries.
	/// @note Written by the kernel, read by user space.
	void const* const m_cqes;


	bool m_in_record_context : 1 = false;
	bool m_has_enter_ext_arg : 1 = false;
	bool m_has_kernel_thread : 1 = false;
	bool m_has_register_wake : 1 = false;

	/// @brief Dynamic size of a single @ref io_uring_sqe.
	uint8_t const m_sqe_size;

	/// @brief Dynamic size of a single @ref io_uring_cqe.
	uint8_t const m_cqe_size;

	/// @brief Shift used for multiplication by @ref m_sqe_size.
	uint8_t const m_sqe_multiply_shift;

	/// @brief Shift used for multiplication by @ref m_cqe_size.
	uint8_t const m_cqe_multiply_shift;

	uint8_t const m_cqe_skip_success_flag;


	/// @brief One past the newest submission queue entry produced by user space.
	/// @note Written by user space, read by the kernel.
	vsm::atomic_ref<uint32_t> const m_k_sq_produce;

	/// @brief One past the newest submission queue entry consumed by the kernel.
	/// @note The value always trails @ref m_k_sq_produce.
	/// @note Written by the kernel, read by user space.
	vsm::atomic_ref<uint32_t const> const m_k_sq_consume;

	/// @brief Maps logical SQE index to physical index in @ref m_sqes.
	/// @note Written by user space, read by user space and the kernel.
	uint32_t* const m_k_sq_array;

	/// @brief One past the newest completion queue entry produced by the kernel.
	/// @note Written by the kernel, read by user space.
	vsm::atomic_ref<uint32_t const> const m_k_cq_produce;

	/// @brief One past the newest completion queue entry consumed by user space.
	/// @note The value always trails @ref m_k_cq_produce.
	/// @note Written by user space, read by the kernel.
	vsm::atomic_ref<uint32_t> const m_k_cq_consume;

	/// @brief Describes the io_uring dynamic state.
	/// @note Written by the kernel, read by user space.
	vsm::atomic_ref<uint32_t const> const m_k_flags;


	/// @brief Size of the submission queue buffer.
	uint32_t const m_sq_size;

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
	uint32_t const m_cq_size;

	uint32_t m_cq_produce;

	/// @brief One past the CQE most recently consumed by user space.
	/// @ref m_k_cq_consume == m_cq_consume <= @ref m_k_cq_produce
	uint32_t m_cq_consume;


	vsm_gcc_diagnostic(push)

	// TODO: Move _io_uring_multiplexer into multiplexer.cpp.
	// GCC warns about the use of hardware_destructive_interference_size due to its ABI-breaking
	// potential. Despite being located in a header, the members of this structure are not accessed
	// by library user code.
	vsm_gcc_diagnostic(ignored "-Winterference-size")

	struct alignas(std::hardware_destructive_interference_size)
	{
		vsm::atomic<bool> wake_requested = false;
		vsm::atomic<bool> cancel_pending = false;

		vsm::intrusive::mpsc_queue<operation_type> cancel_queue;
	}
	m_shared;

	vsm_gcc_diagnostic(pop)


	explicit _io_uring_multiplexer(
		io_uring_params const& setup,
		unique_handle&& io_uring,
		unique_byte_mmap&& sq_ring,
		unique_byte_mmap&& cq_ring,
		unique_void_mmap&& sq_data) noexcept;

	~_io_uring_multiplexer();


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

			friend _io_uring_multiplexer;
		};

		reference set(deadline const deadline) &
		{
			m_timespec = make_timespec<timespec>(deadline);
			return reference(m_timespec, deadline.is_absolute());
		}

	private:
		friend _io_uring_multiplexer;
	};


	class record_context;


	template<typename T>
	class ring_view
	{
		using void_type = vsm::copy_cv_t<T, void>;
		using byte_type = vsm::copy_cv_t<T, std::byte>;

		byte_type* m_ring;
		uint32_t m_mask;
		uint32_t m_multiply_shift;

	public:
		explicit ring_view(
			void_type* const ring,
			uint32_t const mask,
			uint32_t const multiply_shift)
			: m_ring(static_cast<byte_type*>(ring))
			, m_mask(mask)
			, m_multiply_shift(multiply_shift)
		{
		}

		[[nodiscard]] T& operator[](uint32_t const offset) const
		{
			return *reinterpret_cast<T*>(m_ring + ((offset & m_mask) << m_multiply_shift));
		}
	};

	[[nodiscard]] ring_view<io_uring_sqe> get_sqes() const
	{
		return ring_view<io_uring_sqe>(m_sqes.get(), m_sq_size - 1, m_sqe_multiply_shift);
	}

	[[nodiscard]] ring_view<io_uring_cqe const> get_cqes() const
	{
		return ring_view<io_uring_cqe const>(m_cqes, m_cq_size - 1, m_cqe_multiply_shift);
	}


	[[nodiscard]] vsm::result<void> attach_fd(int fd, connector_type& c);
	[[nodiscard]] vsm::result<void> detach_fd(int fd, connector_type& c);


	void enter_record_context()
	{
		vsm_assert(!m_in_record_context &&
			"The I/O recording contexts must have non-overlapping lifetimes.");

		m_in_record_context = true;
	}

	void leave_record_context()
	{
		vsm_assert(m_in_record_context);
		m_in_record_context = false;
	}

	[[nodiscard]] bool flush_cancel_queue();

	[[nodiscard]] bool has_available_sqes() const;

	[[nodiscard]] bool has_pending_sqes() const;
	[[nodiscard]] bool has_pending_cqes() const;

	void release_sqes();
	[[nodiscard]] bool acquire_cqes();

	void reap_cqe(io_uring_cqe const& cqe);
	[[nodiscard]] bool reap_all_cqes();

	[[nodiscard]] vsm::result<void> commit();


	void submit_async_cancel(user_data_ptr user_data);
	void cancel_io_externally_synchronized(operation_type& operation, user_data_ptr user_data);
	void cancel_io_internally_synchronized(operation_type& operation);
	void cancel_io(operation_type& operation, user_data_ptr user_data);

	void wake_poll_thread();
	void wake_poll_thread_reset();

	[[nodiscard]] bool is_kernel_thread_inactive() const;

	[[nodiscard]] vsm::result<int> enter(
		unsigned to_submit,
		unsigned min_complete,
		unsigned flags,
		deadline deadline);

	[[nodiscard]] vsm::result<void> wait_for_sqe(deadline deadline);

	[[nodiscard]] vsm::result<bool> poll(poll_parameters const& args);
};

static_assert(std::is_default_constructible_v<_io_uring_multiplexer::io_slot>);
static_assert(std::is_default_constructible_v<_io_uring_multiplexer::timeout>);


//TODO: The io_uring_multiplexer handle should point directly to the internal object.
class io_uring_multiplexer final : public externally_synchronized
{
public:
	using multiplexer_concept = void;

	using connector_type = _io_uring_multiplexer::connector_type;
	using operation_type = _io_uring_multiplexer::operation_type;
	using io_status_type = _io_uring_multiplexer::io_status_type;

private:
	//TODO: Use acquire_storage/release_storage.
	std::unique_ptr<_io_uring_multiplexer> m_multiplexer;


	struct create_parameters
	{
		io_uring_options options;
		io_uring_multiplexer const* kernel_thread;
		size_t submission_queue_size;
		size_t completion_queue_size;

		void set_argument(explicit_reference_parameter<io_uring::kernel_thread_t>)
		{
			options |= io_uring_options::kernel_thread;
		}

		void set_argument(io_uring::kernel_thread_t const value)
		{
			options |= io_uring_options::kernel_thread;
			kernel_thread = value.value;
		}

		void set_argument(io_uring::submission_queue_size_t const value)
		{
			submission_queue_size = value.value;
		}

		void set_argument(io_uring::completion_queue_size_t const value)
		{
			completion_queue_size = value.value;
		}
	};

	using poll_parameters = _io_uring_multiplexer::poll_parameters;

public:
	[[nodiscard]] static vsm::result<io_uring_multiplexer> create(auto&&... args)
	{
		auto a = create_parameters{};
		(set_argument(a, vsm_forward(args)), ...);
		return _create(a);
	}


	[[nodiscard]] vsm::result<void> attach_fd(int const fd, connector_type& c)
	{
		return m_multiplexer->attach_fd(fd, c);
	}

	[[nodiscard]] vsm::result<void> detach_fd(int const fd, connector_type& c)
	{
		return m_multiplexer->detach_fd(fd, c);
	}

	template<typename Object>
	[[nodiscard]] vsm::result<void> attach_handle(
		native_handle<Object> const& h,
		async_connector<io_uring_multiplexer, Object>& c)
	{
		return m_multiplexer->attach_fd(unwrap_handle(h.platform_handle), c);
	}

	template<typename Object>
	[[nodiscard]] vsm::result<void> detach_handle(
		native_handle<Object> const& h,
		async_connector<io_uring_multiplexer, Object>& c)
	{
		return m_multiplexer->detach_fd(unwrap_handle(h.platform_handle), c);
	}


	void cancel_io(operation_type& operation)
	{
		m_multiplexer->cancel_io(
			operation,
			_io_uring_multiplexer::user_data_ptr(&operation));
	}

	void cancel_io(operation_type& operation, _io_uring_multiplexer::io_slot& slot)
	{
		m_multiplexer->cancel_io(
			operation,
			_io_uring_multiplexer::user_data_ptr(
				&slot,
				_io_uring_multiplexer::user_data_tag::io_slot));
	}


	/// @return True if the multiplexer made any progress.
	[[nodiscard]] vsm::result<bool> poll(auto&&... args)
	{
		return m_multiplexer->poll(make_args<poll_parameters>(vsm_forward(args)...));
	}


	void external_synchronization_acquired() &
	{
		m_multiplexer->external_synchronization_acquired();
	}

	void external_synchronization_released() &
	{
		m_multiplexer->external_synchronization_released();
	}

	[[nodiscard]] bool is_externally_synchronized() const
	{
		return m_multiplexer->is_externally_synchronized();
	}


	using io_slot = _io_uring_multiplexer::io_slot;
	using timeout = _io_uring_multiplexer::timeout;
	using record_context = _io_uring_multiplexer::record_context;

private:
	explicit io_uring_multiplexer(std::unique_ptr<_io_uring_multiplexer> multiplexer) noexcept
		: m_multiplexer(vsm_move(multiplexer))
	{
	}

	static vsm::result<io_uring_multiplexer> _create(create_parameters const& args) noexcept;

	friend vsm::result<bool> tag_invoke(poll_io_t, io_uring_multiplexer& m, auto&&... args)
	{
		return m.m_multiplexer->poll(make_args<poll_parameters>(vsm_forward(args)...));
	}

	friend _io_uring_multiplexer::record_context;
};

} // namespace allio::detail

#include <allio/linux/detail/undef.i>
