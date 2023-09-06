#pragma once

#include <allio/detail/io_parameters.hpp>
#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/atomic.hpp>

#include <cstdint>

namespace allio::detail {

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

template<typename M, typename H>
struct async_handle_storage
	: M::context_type
	, async_handle_data<M, H>
{
};

template<typename M, typename H, typename O>
struct async_operation_data {};

template<typename M, typename H, typename O>
struct async_operation_storage
	: M::operation_storage
	, async_operation_data<M, H, O>
{
	io_parameters_with_result<O> args;

	explicit async_operation_storage(
		io_parameters_with_result<O> const& args,
		async_operation_listener* const listener)
		: M::async_storage(listener)
		, args(args)
	{
	}

	using M::operation_storage::set_result;
};


#if 0
template<typename M, typename H>
struct async_handle_impl {};

template<typename M, typename H>
struct async_handle_facade
{
	using C = async_handle_storage<M, H>;
	using impl = async_handle_impl<M, H>;

	static vsm::result<void> attach(M& m, C& c, H const& h) noexcept
	{
		if constexpr (requires { impl::attach(m, c, h); })
		{
			return impl::attach(m, c, h);
		}
		else
		{
			return m.attach(c, h);
		}
	}

	static vsm::result<void> detach(M& m, C& c, H const& h) noexcept
	{
		if constexpr (requires { impl::detach(m, c, h); })
		{
			return impl::detach(m, c, h);
		}
		else
		{
			return m.detach(c, h);
		}
	}
};

template<typename M, typename H, typename O>
struct async_operation_impl {};

template<typename M, typename H, typename O>
struct async_operation_facade
{
	using impl = async_operation_impl<M, H, O>;
	using S = async_operation_storage<M, H, O>;

	static S initialize(
		io_parameters_with_result<O> const& args,
		async_operation_listener const* const listener)
	{
		return S(args, listener);
	}

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
#endif

} // namespace allio::detail
