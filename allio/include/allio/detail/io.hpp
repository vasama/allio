#pragma once

#include <allio/detail/io_parameters.hpp>
#include <allio/detail/io_result.hpp>
#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

#include <memory>
#include <optional>

namespace allio::detail {

struct bounded_runtime_t;


template<bool IsMutable>
struct _handle_cv;

template<>
struct _handle_cv<0>
{
	template<typename T>
	using type = T const;
};

template<>
struct _handle_cv<1>
{
	template<typename T>
	using type = T;
};

template<typename O>
inline constexpr bool is_mutating = requires { typename O::mutation_tag; };

template<typename O, typename T>
using handle_cv = typename _handle_cv<is_mutating<O>>::template type<T>;


template<typename O>
struct blocking_io_t
{
	template<typename H>
	auto vsm_static_operator_invoke(H& handle, io_parameters_t<O> const& args)
		//requires vsm::tag_invocable<blocking_io_t, H&, io_parameters_t<O> const&>
	{
		//return vsm::tag_invoke(blocking_io_t(), handle, args);
		return tag_invoke(blocking_io_t(), handle, args);
	}
};

template<typename O>
inline constexpr blocking_io_t<O> blocking_io = {};


using io_result = std::optional<std::error_code>;

class operation_base;


class io_status
{
	struct _io_status;

	_io_status* m_status;

public:
	template<typename Status>
	explicit io_status(Status& status)
		: m_status(reinterpret_cast<_io_status*>(&status))
	{
	}

	template<typename Status>
	Status& unwrap() const
	{
		return *reinterpret_cast<Status*>(m_status);
	}
};

class io_callback
{
public:
	virtual void notify(operation_base& s, io_status status) noexcept = 0;

protected:
	io_callback() = default;
	io_callback(io_callback const&) = default;
	io_callback& operator=(io_callback const&) = default;
	~io_callback() = default;
};

class operation_base
{
	io_callback* m_callback;

protected:
	explicit operation_base(io_callback& callback)
		: m_callback(&callback)
	{
	}

	operation_base(operation_base const&) = delete;
	operation_base& operator=(operation_base const&) = delete;

	~operation_base() = default;

	void notify(io_status const status)
	{
		m_callback->notify(*this, status);
	}
};


struct attach_handle_t
{
	template<typename M, typename H, typename C>
	vsm::result<void> vsm_static_operator_invoke(M& m, H const& h, C& c)
		requires vsm::tag_invocable<attach_handle_t, M&, H const&, C&>
	{
		return vsm::tag_invoke(attach_handle_t(), m, h, c);
	}
};
inline constexpr attach_handle_t attach_handle = {};

struct detach_handle_t
{
	template<typename M, typename H, typename C>
	void vsm_static_operator_invoke(M& m, H const& h, C& c)
		requires vsm::tag_invocable<detach_handle_t, M const&, H const&, C&>
	{
		return vsm::tag_invoke(detach_handle_t(), m, h, c);
	}
};
inline constexpr detach_handle_t detach_handle = {};

template<typename M>
struct rebind_handle_t
{
	template<typename T>
	vsm::result<T> vsm_static_operator_invoke(T&& object)
		requires vsm::tag_invocable<rebind_handle_t, T&&>
	{
		return vsm::tag_invoke(rebind_handle_t(), vsm_forward(object));
	}
};
template<typename M>
inline constexpr rebind_handle_t<M> rebind_handle = {};


template<typename O>
struct submit_io_t
{
	template<typename H, typename S>
	auto vsm_static_operator_invoke(H& h, S& s)
		requires vsm::tag_invocable<submit_io_t, H&, S&>
	{
		return vsm::tag_invoke(submit_io_t(), h, s);
	}

	template<typename M, typename H, typename C, typename S>
	auto vsm_static_operator_invoke(M& m, H& h, C& c, S& s)
		requires vsm::tag_invocable<submit_io_t, M&, H&, C&, S&>
	{
		return vsm::tag_invoke(submit_io_t(), m, h, c, s);
	}
};

template<typename O>
inline constexpr submit_io_t<O> submit_io = {};

template<typename O>
struct notify_io_t
{
	template<typename H, typename S>
	auto vsm_static_operator_invoke(H& h, S& s, io_status const status)
		requires vsm::tag_invocable<notify_io_t, H&, S&, io_status>
	{
		return vsm::tag_invoke(notify_io_t(), h, s, status);
	}

	template<typename M, typename H, typename C, typename S>
	auto vsm_static_operator_invoke(M& m, H& h, C& c, S& s, io_status const status)
		requires vsm::tag_invocable<notify_io_t, M&, H&, C&, S&, io_status>
	{
		return vsm::tag_invoke(notify_io_t(), m, h, c, s, status);
	}
};

template<typename O>
inline constexpr notify_io_t<O> notify_io = {};

template<typename O>
struct cancel_io_t
{
	template<typename H, typename S>
	void vsm_static_operator_invoke(H& h, S& s)
		requires vsm::tag_invocable<cancel_io_t, H&, S&>
	{
		return vsm::tag_invoke(cancel_io_t(), h, s);
	}

	template<typename M, typename H, typename C, typename S>
	void vsm_static_operator_invoke(M& m, H& h, C& c, S& s)
		requires vsm::tag_invocable<cancel_io_t, M&, H&, C&, S&>
	{
		return vsm::tag_invoke(cancel_io_t(), m, h, c, s);
	}
};

template<typename O>
inline constexpr cancel_io_t<O> cancel_io = {};


struct poll_io_t
{
	template<typename Multiplexer, typename... Args>
	vsm::result<bool> vsm_static_operator_invoke(Multiplexer&& m, Args&&... args)
		requires vsm::tag_invocable<poll_io_t, Multiplexer&&, Args&&...>
	{
		return vsm::tag_invoke(poll_io_t(), vsm_forward(m), vsm_forward(args)...);
	}
};
inline constexpr poll_io_t poll_io = {};


template<typename M, typename H>
struct connector_impl;

template<typename M, typename H>
class connector
	: public M::connector_type
	, public connector_impl<M, H>
{
	using impl_type = connector_impl<M, H>;
	static_assert(std::is_default_constructible_v<impl_type>);
	friend impl_type;
};

template<typename M, typename H>
using connector_t = connector<M, H>;


template<typename M, typename H, typename O>
struct operation_impl;

template<typename M, typename H, typename O>
class operation
	: public M::operation_type
	, public operation_impl<M, H, O>
{
	using impl_type = operation_impl<M, H, O>;
	static_assert(std::is_default_constructible_v<impl_type>);
	friend impl_type;

	using params_type = io_parameters_t<O>;
	vsm_no_unique_address params_type args;

public:
	template<std::convertible_to<params_type> Args>
	explicit operation(io_callback& callback, Args&& args)
		: M::operation_type(callback)
		, args(vsm_forward(args))
	{
	}

private:
	using N = handle_cv<O, typename H::native_type>;
	using C = handle_cv<O, connector_t<M, H>>;
	using S = operation;

	template<typename MultiplexerHandle>
	friend auto tag_invoke(
		submit_io_t<O>,
		MultiplexerHandle const& m,
		N& h,
		C& c,
		S& s)
		//requires requires { impl_type::submit(m, h, c, s); }
	{
		return impl_type::submit(
			m,
			h,
			c,
			s);
	}

	template<typename MultiplexerHandle>
	friend auto tag_invoke(
		notify_io_t<O>,
		MultiplexerHandle const& m,
		N& h,
		C& c,
		S& s,
		io_status const status)
		//requires requires { impl_type::notify(m, h, c, s, status); }
	{
		return impl_type::notify(m, h, c, s, status);
	}

	template<typename MultiplexerHandle>
	friend void tag_invoke(
		cancel_io_t<O>,
		MultiplexerHandle const& m,
		N const& h,
		C const& c,
		S& s)
		requires requires { impl_type::cancel(m, h, c, s); }
	{
		return impl_type::cancel(m, h, c, s);
	}
};

template<typename M, typename H, typename O>
using operation_t = operation<M, H, O>;


template<typename M, typename H, typename O>
	requires std::same_as<typename O::runtime_tag, bounded_runtime_t>
struct operation_impl<M, H, O>
{
	using N = handle_cv<O, typename H::native_type>;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using R = io_result_t<O, H, M>;

	static io_result2<R> submit(M&, N& h, C const&, S& s)
	{
		return H::blocking_io(O(), h, s.args);
	}

	static io_result2<R> notify(M&, N& h, C const&, S&, io_status)
	{
		vsm_unreachable();
	}

	static void cancel(M&, N const& h, C const&, S&)
	{
	}
};


template<typename M>
using multiplexer_t = typename std::pointer_traits<M>::element_type;

} // namespace allio::detail
