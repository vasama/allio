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

struct producer_t;
struct consumer_t;
struct modifier_t;

struct bounded_runtime_t;


template<typename O>
concept mutation = requires { typename O::mutation_tag; };

template<typename O>
concept producer = mutation<O> && std::is_same_v<typename O::mutation_tag, producer_t>;

template<typename O>
concept consumer = mutation<O> && std::is_same_v<typename O::mutation_tag, consumer_t>;

template<typename O>
concept modifier = mutation<O> && std::is_same_v<typename O::mutation_tag, modifier_t>;

template<typename O>
concept observer = !mutation<O>;


template<bool IsMutable>
struct _handle_const;

template<>
struct _handle_const<0>
{
	template<typename T>
	using type = T const;
};

template<>
struct _handle_const<1>
{
	template<typename T>
	using type = T;
};

template<typename O, typename T>
using handle_const_t = typename _handle_const<mutation<O>>::template type<T>;


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


template<typename Handler, typename Status>
using basic_io_callback = void(Handler& handler, Status&& status) noexcept;

template<typename Status>
class basic_io_handler
{
	using callback_type = basic_io_callback<basic_io_handler, Status>;

	callback_type* m_callback;

public:
	explicit basic_io_handler(callback_type& callback)
		: m_callback(callback)
	{
	}

	void notify(Status&& status) & noexcept
	{
		m_callback(*this, vsm_move(status));
	}
};

template<typename Status, typename Handler>
class basic_io_handler_base : protected basic_io_handler<Status>
{
	using io_handler_type = basic_io_handler<Status>;

protected:
	basic_io_handler_base()
		: io_handler_type(_notify)
	{
	}

	basic_io_handler_base(basic_io_handler_base const&) = default;
	basic_io_handler_base& operator=(basic_io_handler_base const&) = default;
	~basic_io_handler_base() = default;

private:
	static void _notify(io_handler_type& self, Status&& status) noexcept
	{
		static_cast<Handler&>(static_cast<io_handler_type&>(self)).notify(vsm_move(status));
	}
};

template<typename Multiplexer>
using io_handler = basic_io_handler<typename Multiplexer::io_status_type>;

template<typename Multiplexer, typename Handler>
using io_handler_base = basic_io_handler_base<typename Multiplexer::io_status_type, Handler>;


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

template<typename To>
struct rebind_handle_t
{
	template<vsm::any_cvref_of<To> From>
	friend From&& tag_invoke(From&& from)
	{
		return vsm_forward(from);
	}

	template<typename From, typename MultiplexerHandle>
	vsm::result<To> vsm_static_operator_invoke(From&& from, MultiplexerHandle&& multiplexer)
		requires vsm::tag_invocable<rebind_handle_t, From&&, MultiplexerHandle&&>
	{
		return vsm::tag_invoke(rebind_handle_t(), vsm_forward(from), vsm_forward(multiplexer));
	}
};
template<typename To>
inline constexpr rebind_handle_t<To> rebind_handle = {};


struct submit_io_t
{
	template<typename H, typename S, typename Args, typename Handler>
	auto vsm_static_operator_invoke(H& h, S& s, Args const& args, Handler& handler)
		requires vsm::tag_invocable<submit_io_t, H&, S&, Args const&, Handler&>
	{
		return vsm::tag_invoke(submit_io_t(), h, s, args, handler);
	}

	template<typename M, typename H, typename C, typename S, typename Args, typename Handler>
	auto vsm_static_operator_invoke(M& m, H& h, C& c, S& s, Args const& args, Handler& handler)
		requires vsm::tag_invocable<submit_io_t, M&, H&, C&, S&, Args const&, Handler&>
	{
		return vsm::tag_invoke(submit_io_t(), m, h, c, s, args, handler);
	}
};
inline constexpr submit_io_t submit_io = {};

struct notify_io_t
{
	template<typename H, typename S, typename Args, typename Status>
	auto vsm_static_operator_invoke(H& h, S& s, Args const& args, Status&& status)
		requires vsm::tag_invocable<notify_io_t, H&, S&, Args const&, Status&&>
	{
		return vsm::tag_invoke(notify_io_t(), h, s, args, vsm_forward(status));
	}

	template<typename M, typename H, typename C, typename S, typename Args, typename Status>
	auto vsm_static_operator_invoke(M& m, H& h, C& c, S& s, Args const& args, Status&& status)
		requires vsm::tag_invocable<notify_io_t, M&, H&, C&, S&, Args const&, Status&&>
	{
		return vsm::tag_invoke(notify_io_t(), m, h, c, s, args, vsm_forward(status));
	}
};
inline constexpr notify_io_t notify_io = {};

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
inline constexpr cancel_io_t cancel_io = {};


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


struct connector_base
{
	template<std::derived_from<connector_base> C>
	friend auto tag_invoke(
		attach_handle_t,
		auto const& m,
		auto const& h,
		C& c)
	{
		return C::attach(m, h, c);
	}

	template<std::derived_from<connector_base> C>
	friend auto tag_invoke(
		detach_handle_t,
		auto const& m,
		auto const& h,
		C& c)
	{
		return C::detach(m, h, c);
	}
};

template<typename M, typename H>
struct connector;

template<typename M, typename H>
using connector_t = connector<M, H>;


struct operation_base
{
	template<std::derived_from<operation_base> S, typename IoStatus>
	friend auto tag_invoke(
		submit_io_t,
		auto& m,
		auto& h,
		std::derived_from<connector_base> auto& c,
		S& s,
		auto const& args,
		basic_io_handler<IoStatus>& handler)
		//requires requires { impl_type::submit(m, h, c, s); }
	{
		return S::submit(
			m,
			h,
			c,
			s,
			args,
			handler);
	}

	template<std::derived_from<operation_base> S>
	friend auto tag_invoke(
		notify_io_t,
		auto& m,
		auto& h,
		std::derived_from<connector_base> auto& c,
		S& s,
		auto const& args,
		auto&& status)
		//requires requires { impl_type::notify(m, h, c, s, status); }
	{
		return S::notify(
			m,
			h,
			c,
			s,
			args,
			vsm_forward(status));
	}

	template<std::derived_from<operation_base> S>
	friend void tag_invoke(
		cancel_io_t,
		auto& m,
		auto const& h,
		std::derived_from<connector_base> auto const& c,
		S& s)
		//requires requires { impl_type::cancel(m, h, c, s); }
	{
		return S::cancel(
			m,
			h,
			c,
			s);
	}
};

template<typename M, typename H, typename O>
struct operation;

template<typename M, typename H, typename O>
using operation_t = operation<M, H, O>;


template<typename M, typename H, observer O>
	requires std::is_same_v<typename O::runtime_tag, bounded_runtime_t>
struct operation<M, H, O>
{
	using N = typename H::native_type const;
	using C = connector_t<M, H> const;
	using S = operation_t<M, H, O>;
	using R = io_result_t<O, H, M>;

	static io_result<R> submit(M&, N& h, C&, S& s)
	{
		return blocking_io<O>(h, s.args);
	}

	static io_result<R> notify(M&, N& h, C&, S& s, typename M::io_status_type&& status)
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
