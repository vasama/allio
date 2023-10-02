#pragma once

#include <allio/detail/handles/platform_handle.hpp>
#include <allio/detail/io.hpp>
#include <allio/linux/timespec.hpp>
#include <allio/multiplexer.hpp>

#include <vsm/result.hpp>
#include <vsm/tag_ptr.hpp>

#include <allio/linux/detail/undef.i>

struct io_uring_sqe;
struct io_uring_cqe;

namespace allio::detail {

class io_uring_multiplexer final
{
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

private:
	using io_data_ptr = vsm::tag_ptr<io_data, uintptr_t, 1>;

public:
	class io_data_ref : io_data_ptr
	{
	public:
		io_data_ref(async_operation_storage& storage)
			: io_data_ptr(&storage, 0)
		{
		}

		io_data_ref(io_slot& slot)
			: io_data_ptr(&slot, 1)
		{
		}

		friend bool operator==(io_data_ref const&, io_data_ref const&) = default;

	private:
		friend class io_uring_multiplexer;
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

public:
	static bool is_supported();


	#define allio_io_uring_multiplexer_create_parameters(type, data, ...) \
		type(::allio::detail::io_uring_multiplexer, create_parameters) \
		data(bool,                                          enable_kernel_thread,           false) \
		data(::allio::detail::io_uring_multiplexer*,        share_kernel_thread,            nullptr) \

	allio_interface_parameters(allio_io_uring_multiplexer_create_parameters);

	template<parameters<create_parameters> P = create_parameters::interface>
	[[nodiscard]] static vsm::result<io_uring_multiplexer> create(P const& args = {})
	{
		return _create(args);
	}


	/// @return True if the multiplexer made any progress.
	template<parameters<poll_parameters> P = poll_parameters::interface>
	[[nodiscard]] vsm::result<bool> poll(P const& args = {})
	{
		return _poll(args);
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

	class submission_context
	{
		io_uring_multiplexer& m_multiplexer;

		uint32_t m_sq_acquire;
		uint32_t m_cq_available;

	public:
		explicit submission_context(io_uring_multiplexer& multiplexer)
			: m_multiplexer(&multiplexer)
			, m_sq_acquire(multiplexer.m_sq_acquire)
			, m_cq_available(multiplexer.m_cq_available)
		{
			vsm_assert(
				!vsm::any_flags(
					std::exchange(
						m_multiplexer->m_flags,
						m_multiplexer->m_flags | flags::submit_lock),
					flags::submit_lock));
		}

		submission_context(submission_context const&) = delete;
		submission_context& operator=(submission_context const&) = delete;

		~submission_context()
		{
			vsm_assert(
				vsm::any_flags(
					std::exchange(
						m_multiplexer->m_flags,
						m_multiplexer->m_flags & ~flags::submit_lock),
					flags::submit_lock));
		}


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

		vsm::result<void> commit()
		{
			m_multiplexer->m_sq_acquire = m_sq_acquire;
			m_multiplexer->m_cq_available = m_cq_available;
			return m_multiplexer->commit_submission();
		}

	private:
		vsm::result<io_uring_sqe*> acquire_sqe(io_data_ref data);

		static bool check_sqe_user_data(io_uring_sqe const& sqe, io_data_ref data) noexcept;
		static bool check_sqe_flags(io_uring_sqe const& sqe) noexcept;

		friend class io_uring_multiplexer;
	};

	vsm::result<void> record_io(std::invocable<submission_context&> auto&& record)
	{
		submission_context context(*this);
		vsm_try_void(vsm_forward(record)(context));
		return context.commit();
	}

	vsm::result<void> record_io(io_data_ref const data, std::invocable<io_uring_sqe&> auto&& initialize_sqe)
	{
		return record_io([&](submission_context& context) -> vsm::result<void>
		{
			return context.push(data, vsm_forward(initialize_sqe));
		});
	}

private:
	explicit io_uring_multiplexer();


	[[nodiscard]] static vsm::result<io_uring_multiplexer> _create(create_parameters const& args);


	[[nodiscard]] vsm::result<void> _attach_handle(native_platform_handle handle, connector_type& c);
	void _detach_handle(native_platform_handle handle, connector_type& c);

	template<std::derived_from<_platform_handle> H>
	friend vsm::result<void> tag_invoke(attach_handle_t, io_uring_multiplexer& m, H const& h, connector_type& c)
	{
		return m._attach_handle(h.get_platform_handle(), c);
	}

	template<std::derived_from<_platform_handle> H>
	friend void tag_invoke(detach_handle_t, io_uring_multiplexer& m, H const& h, connector_type& c)
	{
		return m._detach_handle(h.get_platform_handle(), c);
	}


	[[nodiscard]] uint32_t _reap_completion_queue();

	[[nodiscard]] vsm::result<bool> _poll(poll_parameters const& args);
};

} // namespace allio::detail

#include <allio/linux/detail/undef.i>
