#pragma once

#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/atomic.hpp>

#include <cstdint>

namespace allio {

class async_operation;

enum class async_operation_status : uint8_t
{
	// The asynchronous operation has been submitted and is now in progress.
	submitted                   = 1,

	// The asynchronous operation has been completed.
	// Note that cancellation is a kind of completion.
	completed                   = 2,

	// The asynchronous operation has been concluded.
	// The multiplexer no longer borrows the operation state and it may be destroyed.
	// Note that if a listener is used, the operation state is still borrowed until
	// async_operation_listener::concluded() is invoked on this operation state.
	concluded                   = 3,
};

class async_operation_listener
{
public:
	virtual void submitted(async_operation& operation) {}
	virtual void completed(async_operation& operation) {}
	virtual void concluded(async_operation& operation) {}
};

class async_operation
{
	async_operation_listener* m_listener;
	vsm::atomic<async_operation_status> m_status;
	std::error_code m_result;

public:
	async_operation_listener* get_listener() const
	{
		return m_listener;
	}

	async_operation_status get_status() const
	{
		return m_status.load(std::memory_order_acquire);
	}

	bool is_submitted() const
	{
		return get_status() >= async_operation_status::submitted;
	}

	bool is_completed() const
	{
		return get_status() >= async_operation_status::completed;
	}

	bool is_concluded() const
	{
		return get_status() >= async_operation_status::concluded;
	}

	std::error_code get_result() const
	{
		vsm_assert(is_completed()); //PRECONDITION
		return m_result;
	}

protected:
	explicit async_operation(async_operation_listener* const listener)
		: m_listener(listener)
	{
	}

	async_operation(async_operation const&) = delete;
	async_operation& operator=(async_operation const&) = delete;
	~async_operation() = default;

	void set_result(std::error_code const result)
	{
		m_result = result;
	}

	void set_status(async_operation_status const status)
	{
		vsm_assert(status > m_status.load(std::memory_order_relaxed));
		m_status.store(status, std::memory_order_release);
	}
};

class async_operation_parameters
{
protected:
	async_operation_parameters() = default;
	async_operation_parameters(async_operation_parameters const&) = default;
	async_operation_parameters& operator=(async_operation_parameters const&) = default;
	~async_operation_parameters() = default;
};


template<typename M, typename H>
struct async_handle_data {};

template<typename M, typename H, typename O>
struct async_operation_data {};


template<typename M, typename H>
struct async_handle_storage
	: M::handle_type
	, async_handle_data<M, H>
{
};

template<typename M, typename H, typename O>
struct async_operation_storage
	: M::async_storage
	, async_operation_data<M, H, O>
{
	io_parameters_with_result<O> args;

	explicit async_operation_storage(
		io_parameters_with_result<O> const& args,
		async_operation_listener* const listener)
		: B(listener)
		, args(args)
	{
	}

	using async_operation::set_result;
};


template<typename M, typename H>
struct async_handle_impl {};

template<typename M, typename H>
struct async_handle_facade
{
	using impl = async_handle_impl<M, H>;
	using context = async_handle_storage<M, H>;

	static vsm::result<void> attach(M& m, H const& h, context& c) noexcept
	{
		vsm_try_void(M::attach(m, h, c));

		if constexpr (requires { impl::attach(m, h, c); })
		{
			if (auto const r = impl::attach(m, h, c); !r)
			{
				unrecoverable(M::detach(m, h, c));
				return vsm::unexpected(r.error());
			}
		}

		return {};
	}

	static void detach(M& m, H const& h, context& c) noexcept
	{
		if constexpr (requires { impl::detach(m, h, c); })
		{
			unrecoverable(impl::detach(m, h, c));
		}
		
		unrecoverable(M::detach(m, h, c));
	}
};


template<typename M, typename H, typename O>
struct async_operation_impl {};

template<typename M, typename H, typename O>
struct async_operation_facade
{
	using impl = async_operation_impl<M, H, O>;
	using S = async_operation_storage<M, H, O>;

	static vsm::result<void> submit(M& m, S& s) noexcept
	{
		if constexpr (requires { impl::submit(m, s); })
		{
			return impl::submit(m, s);
		}
		else
		{
			return vsm::unexpected(error::unsupported_operation);
		}
	}

	static vsm::result<void> cancel(M& m, S& s) noexcept
	{
		if constexpr (requires { impl::cancel(m, s); })
		{
			return impl::cancel(m, s);
		}
		else
		{
			return vsm::unexpected(error::unsupported_operation);
		}
	}
};

} // namespace allio
