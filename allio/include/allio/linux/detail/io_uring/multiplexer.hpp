#pragma once

#include <allio/detail/handles/platform_handle.hpp>
#include <allio/detail/io.hpp>
#include <allio/linux/detail/unique_fd.hpp>
#include <allio/linux/timespec.hpp>
#include <allio/multiplexer.hpp>

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

	struct tag_t
	{
		struct value_t
		{
			io_uring_multiplexer const* value;
		};

		value_t vsm_static_operator_invoke(io_uring_multiplexer const* const kernel_thread)
		{
			return { kernel_thread };
		}

		friend void tag_invoke(set_argument_t, kernel_thread_t& args, tag_t)
		{
			args.kernel_thread = nullptr;
		}

		friend void tag_invoke(set_argument_t, kernel_thread_t& args, value_t const& value)
		{
			args.kernel_thread = value.value;
		}
	};
};
inline constexpr kernel_thread_t::tag_t kernel_thread = {};

} // namespace io_uring

class io_uring_multiplexer final
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

	// Intentionally incomplete to prevent arithmetic.
	struct sqe_placeholder;
	struct cqe_placeholder;

	enum class flags : uint32_t
	{
		enter_ext_arg                   = 1 << 0,
		kernel_thread                   = 1 << 1,
		auto_submit                     = 1 << 2,
		record_lock                     = 1 << 3,
	};
	vsm_flag_enum_friend(flags);

public:
	/// @brief Base class for objects bound to concrete io uring operations.
	/// @note Over-aligned for pointer tagging via vsm::tag_ptr. See @ref io_data_ptr.
	class alignas(4) io_data
	{
	protected:
		io_data() = default;
		io_data(io_data const&) = default;
		io_data& operator=(io_data const&) = default;
		~io_data() = default;
	};

	class connector_type
	{
		int file_index;
	};

	class operation_type
		: public io_data
		, public operation_base
	{
	protected:
		using operation_base::operation_base;
		operation_type(operation_type const&) = default;
		operation_type& operator=(operation_type const&) = default;
		~operation_type() = default;

	private:
		using operation_base::notify;

		friend class io_uring_multiplexer;
	};


	class io_slot : public io_data
	{
		operation_type* m_operation = nullptr;

	public:
		void set_operation(operation_type& operation) &
		{
			m_operation = &operation;
		}

	private:
		friend class io_uring_multiplexer;
	};

	class io_data_ref : vsm::tag_ptr<io_data, bool, true>
	{
	public:
		io_data_ref(operation_type& operation)
			: tag_ptr(&operation, false)
		{
		}

		io_data_ref(io_slot& slot)
			: tag_ptr(&slot, true)
		{
		}

		friend bool operator==(io_data_ref const&, io_data_ref const&) = default;

	private:
		friend class io_uring_multiplexer;
	};

	struct io_status_type
	{
		io_slot* slot;
		int32_t result;
		uint32_t flags;
	};

	static io_status_type const& unwrap_io_status(io_status const status)
	{
		return status.unwrap<io_status_type>();
	}

private:
	class mmap_deleter
	{
		//TODO: Eliminate the borrow state and just store null instead?
		static constexpr size_t borrow_value = static_cast<size_t>(-1);

		size_t m_size;

	public:
		mmap_deleter() = default;

		explicit mmap_deleter(size_t const size)
			: m_size(size)
		{
		}

		static mmap_deleter borrow()
		{
			return mmap_deleter(borrow_value);
		}

		void operator()(void const* const addr) const
		{
			if (m_size != borrow_value)
			{
				release(const_cast<void*>(addr), m_size);
			}
		}

	private:
		static void release(void* addr, size_t size);
	};

	template<typename T>
	using unique_mmap = std::unique_ptr<T, mmap_deleter>;

	using unique_byte_mmap = unique_mmap<std::byte>;
	using unique_sqes_mmap = unique_mmap<sqe_placeholder>;


	/// @brief Unique owner of the io uring kernel object.
	unique_fd m_io_uring;

	/// @brief Unique owner of the mmapped region containing the submission queue indices.
	unique_byte_mmap m_sq_mmap;

	/// @brief Unique owner of the mmapped region containing the completion queue entries.
	unique_byte_mmap m_cq_mmap;

	/// @brief Ring of submission queue entries.
	/// @note Written by user space, read by the kernel.
	unique_sqes_mmap m_sqes;

	/// @brief Ring of completion queue entries.
	/// @note Written by the kernel, read by user space.
	cqe_placeholder* m_cqes;


	flags m_flags;

	/// @brief Dynamic size of a single @ref io_uring_sqe.
	uint8_t m_sqe_size;

	/// @brief Dynamic size of a single @ref io_uring_cqe.
	uint8_t m_cqe_size;

	/// @brief Shift used for multiplication by @ref m_sqe_size.
	uint8_t m_sqe_multiply_shift;

	/// @brief Shift used for multiplication by @ref m_cqe_size.
	uint8_t m_cqe_multiply_shift;


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

	/// @brief One past the CQE most recently consumed by user space.
	/// @ref m_k_cq_consume == m_cq_consume <= @ref m_k_cq_produce
	uint32_t m_cq_consume;


	struct create_t
	{
		using required_params_type = no_parameters_t;
		using optional_params_type = io_uring::kernel_thread_t;
	};

	struct poll_t
	{
		using required_params_type = no_parameters_t;
		using optional_params_type = deadline_t;
	};

public:
	[[nodiscard]] static vsm::result<io_uring_multiplexer> create(auto&&... args)
	{
		return _create(io_arguments_t<create_t>()(vsm_forward(args)...));
	}


	/// @return True if the multiplexer made any progress.
	[[nodiscard]] vsm::result<bool> poll(auto&&... args)
	{
		return _poll(io_arguments_t<poll_t>()(vsm_forward(args)...));
	}


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
			timeout const* m_timeout;
			bool m_absolute;

		public:
			explicit reference(timeout const&& timeout, bool absolute) = delete;

			explicit reference(timeout const& timeout, bool const absolute)
				: m_timeout(&timeout)
				, m_absolute(absolute)
			{
			}

		private:
			friend class io_uring_multiplexer;
		};

		reference set(deadline const deadline) &
		{
			m_timespec = make_timespec<timespec>(deadline);
			return reference(*this, deadline.is_absolute());
		}

	private:
		friend class io_uring_multiplexer;
	};

	class record_context
	{
		io_uring_multiplexer& m_multiplexer;

		uint32_t m_sq_acquire;
		uint32_t m_cq_free;

	public:
		vsm::result<void> push(io_data_ref const data, std::invocable<io_uring_sqe&> auto&& initialize_sqe)
		{
			vsm_try(sqe, acquire_sqe(data));
			vsm_forward(initialize_sqe)(*sqe);

			// Users are not allowed to set the user_data field.
			vsm_assert(check_sqe_user_data(*sqe, data) &&
				"Use of io_uring_sqe::user_data is not allowed.");

			// Users are not allowed to set certain sensitive flags.
			vsm_assert(check_sqe_flags(*sqe) &&
				"Use of some io_uring_sqe::flags is restricted.");

			return {};
		}

		vsm::result<void> push_linked_timeout(timeout::reference timeout);

	private:
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


		vsm::result<io_uring_sqe*> acquire_sqe(io_data_ref data);

		static bool check_sqe_user_data(io_uring_sqe const& sqe, io_data_ref data) noexcept;
		static bool check_sqe_flags(io_uring_sqe const& sqe) noexcept;

		vsm::result<void> commit();

		friend class io_uring_multiplexer;
	};

	vsm::result<void> record_io(std::invocable<record_context&> auto&& record)
	{
		record_context context(*this);
		vsm_try_void(vsm_forward(record)(context));
		return context.commit();
	}

	vsm::result<void> record_io(io_data_ref const data, std::invocable<io_uring_sqe&> auto&& initialize_sqe)
	{
		return record_io([&](record_context& context) -> vsm::result<void>
		{
			return context.push(data, vsm_forward(initialize_sqe));
		});
	}

private:
	explicit io_uring_multiplexer(
		unique_fd&& io_uring,
		unique_byte_mmap&& sq_ring,
		unique_byte_mmap&& cq_ring,
		unique_sqes_mmap&& sq_data,
		io_uring_params const& setup);


	[[nodiscard]] static vsm::result<io_uring_multiplexer> _create(io_parameters_t<create_t> const& args);


	[[nodiscard]] vsm::result<void> _attach_handle(native_platform_handle handle, connector_type& c);
	void _detach_handle(native_platform_handle handle, connector_type& c);

	template<std::derived_from<platform_handle> H>
	friend vsm::result<void> tag_invoke(attach_handle_t, io_uring_multiplexer& m, H const& h, connector_type& c)
	{
		return m._attach_handle(h.get_platform_handle(), c);
	}

	template<std::derived_from<platform_handle> H>
	friend void tag_invoke(detach_handle_t, io_uring_multiplexer& m, H const& h, connector_type& c)
	{
		return m._detach_handle(h.get_platform_handle(), c);
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


	[[nodiscard]] uint32_t _reap_completion_queue();

	[[nodiscard]] vsm::result<bool> _poll(io_parameters_t<poll_t> const& args);
};

} // namespace allio::detail

#include <allio/linux/detail/undef.i>
