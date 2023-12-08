#pragma once

#include <allio/detail/io_result.hpp>
#include <allio/detail/object_concepts.hpp>
#include <allio/detail/parameters.hpp>
#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/concepts.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

namespace allio::detail {

struct producer_t;
struct consumer_t;
struct modifier_t;

struct bounded_runtime_t;


//TODO: Rename to operation
template<typename Operation>
concept operation_c = requires { typename Operation::operation_concept; };

template<typename Operation>
concept observer =
	operation_c<Operation> &&
	std::is_void_v<typename Operation::operation_concept>;

template<typename Operation>
concept mutation =
	operation_c<Operation> &&
	!std::is_void_v<typename Operation::operation_concept>;

template<typename Operation>
concept producer =
	mutation<Operation> &&
	std::is_same_v<typename Operation::operation_concept, producer_t> &&
	std::is_void_v<typename Operation::result_type>;

template<typename Operation>
concept consumer =
	mutation<Operation> &&
	std::is_same_v<typename Operation::operation_concept, consumer_t>;

template<typename Operation>
concept modifier =
	mutation<Operation> &&
	std::is_same_v<typename Operation::operation_concept, modifier_t>;


template<object Object, operation_c Operation, optional_multiplexer_handle_for<Object> MultiplexerHandle>
auto _io_result()
{
	if constexpr (requires { typename Operation::result_type; })
	{
		return vsm_declval(typename Operation::result_type);
	}
	else
	{
		return vsm_declval(typename Operation::template result_type_template<Object, MultiplexerHandle>);
	}
}

template<object Object, typename Operation, optional_multiplexer_handle_for<Object> MultiplexerHandle = void>
using io_result_t = decltype(_io_result<Object, Operation, MultiplexerHandle>());


template<typename Required, typename Optional>
struct _io_parameters : Required, Optional
{
	static_assert(std::is_default_constructible_v<Optional>);

	template<typename... Args>
	explicit _io_parameters(Args&&... args)
		requires requires { Required{ vsm_forward(args)... }; }
		: Required{ vsm_forward(args)... }
		, Optional{}
	{
	}
};

template<>
struct _io_parameters<no_parameters_t, no_parameters_t> {};

template<bool IsExtended>
struct _extended_io_parameters;

template<>
struct _extended_io_parameters<0>
{
	template<typename P, object Object, operation_c Operation>
	using type = P;
};

template<>
struct _extended_io_parameters<1>
{
	template<bool IsNoParameters>
	struct parameters;

	template<typename P, typename Extended>
	using _type = typename parameters<std::is_same_v<Extended, no_parameters_t>>::template type<P, Extended>;

	template<typename P, object Object, operation_c Operation>
	using type = _type<P, typename Operation::template extended_params_template<Object>>;
};

template<>
struct _extended_io_parameters<1>::parameters<0>
{
	template<typename P, typename Extended>
	struct type : P, Extended
	{
		using P::P;
	};
};

template<>
struct _extended_io_parameters<1>::parameters<1>
{
	template<typename P, typename Extended>
	using type = P;
};

template<typename Object, typename Operation>
concept has_extended_parameters = requires
{
	typename Operation::template extended_params_template<Object>;
};

template<typename P, object Object, operation_c Operation>
using extended_io_parameters_t = typename _extended_io_parameters<
	has_extended_parameters<Object, Operation>
>::template type<P, Object, Operation>;

template<object Object, operation_c Operation>
using io_parameters_t = extended_io_parameters_t<
	_io_parameters<
		typename Operation::required_params_type,
		typename Operation::optional_params_type>,
	Object,
	Operation>;

template<object Object, operation_c Operation, typename... Args>
[[nodiscard]] auto make_io_args(Args&&... required_args)
{
	using parameters_type = io_parameters_t<Object, Operation>;
	return [&required_args...] vsm_lambda_nodiscard (auto&&... optional_args) -> parameters_type
	{
		parameters_type arguments{ vsm_forward(required_args)... };
		(set_argument(arguments, vsm_forward(optional_args)), ...);
		return arguments;
	};
}


template<bool IsMutation>
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

template<operation_c Operation, typename T>
using handle_const_t = typename _handle_const<mutation<Operation>>::template type<T>;


template<object Object, operation_c Operation>
struct blocking_io_t
{
	template<typename Handle>
	auto vsm_static_operator_invoke(Handle& h, io_parameters_t<Object, Operation> const& a)
		requires vsm::tag_invocable<blocking_io_t, Handle&, io_parameters_t<Object, Operation> const&>
	{
		return vsm::tag_invoke(blocking_io_t(), h, a);
	}
};

template<object Object, operation_c Operation>
inline constexpr blocking_io_t<Object, Operation> blocking_io = {};


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

template<multiplexer Multiplexer>
using io_handler = basic_io_handler<typename Multiplexer::io_status_type>;

template<multiplexer Multiplexer, typename Handler>
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
	vsm::result<void> vsm_static_operator_invoke(M& m, H const& h, C& c)
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
	friend From&& tag_invoke(rebind_handle_t, From&& from, auto&&...)
	{
		return vsm_forward(from);
	}

	template<typename From, typename... Args>
	vsm::result<To> vsm_static_operator_invoke(From&& from, Args&&... args)
		requires vsm::tag_invocable<rebind_handle_t, From&&, Args&&...>
	{
		return vsm::tag_invoke(rebind_handle_t(), vsm_forward(from), vsm_forward(args)...);
	}
};
template<typename To>
inline constexpr rebind_handle_t<To> rebind_handle = {};


struct submit_io_t
{
	template<typename H, typename S, typename A, typename Handler>
	auto vsm_static_operator_invoke(H& h, S& s, A const& a, Handler& handler)
		requires vsm::tag_invocable<submit_io_t, H&, S&, A const&, Handler&>
	{
		return vsm::tag_invoke(submit_io_t(), h, s, a, handler);
	}

	template<typename M, typename H, typename C, typename S, typename A, typename Handler>
	auto vsm_static_operator_invoke(M& m, H& h, C& c, S& s, A const& a, Handler& handler)
		requires vsm::tag_invocable<submit_io_t, M&, H&, C&, S&, A const&, Handler&>
	{
		return vsm::tag_invoke(submit_io_t(), m, h, c, s, a, handler);
	}
};
inline constexpr submit_io_t submit_io = {};

struct notify_io_t
{
	template<typename H, typename S, typename A, typename Status>
	auto vsm_static_operator_invoke(H& h, S& s, A const& a, Status&& status)
		requires vsm::tag_invocable<notify_io_t, H&, S&, A const&, Status&&>
	{
		return vsm::tag_invoke(notify_io_t(), h, s, a, vsm_forward(status));
	}

	template<typename M, typename H, typename C, typename S, typename A, typename Status>
	auto vsm_static_operator_invoke(M& m, H& h, C& c, S& s, A const& a, Status&& status)
		requires vsm::tag_invocable<notify_io_t, M&, H&, C&, S&, A const&, Status&&>
	{
		return vsm::tag_invoke(notify_io_t(), m, h, c, s, a, vsm_forward(status));
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


struct async_connector_base
{
	template<std::derived_from<async_connector_base> Connector>
	friend vsm::result<void> tag_invoke(
		attach_handle_t,
		auto const& multiplexer,
		auto const& handle,
		Connector& connector)
		requires requires { Connector::attach(multiplexer, handle, connector); }
	{
		return Connector::attach(multiplexer, handle, connector);
	}

	template<std::derived_from<async_connector_base> Connector>
	friend vsm::result<void> tag_invoke(
		detach_handle_t,
		auto const& multiplexer,
		auto const& handle,
		Connector& connector)
		requires requires { Connector::detach(multiplexer, handle, connector); }
	{
		return Connector::detach(multiplexer, handle, connector);
	}
};

template<typename M, typename H>
struct async_connector;

template<multiplexer Multiplexer, object Object>
using async_connector_t = async_connector<Multiplexer, Object>;


struct async_operation_base
{
	template<std::derived_from<async_operation_base> S, typename IoStatus>
	friend auto tag_invoke(
		submit_io_t,
		auto& m,
		auto& h,
		std::derived_from<async_connector_base> auto& c,
		S& s,
		auto const& a,
		basic_io_handler<IoStatus>& handler)
		//requires requires { impl_type::submit(m, h, c, s); }
	{
		return S::submit(
			m,
			h,
			c,
			s,
			a,
			handler);
	}

	template<std::derived_from<async_operation_base> S>
	friend auto tag_invoke(
		notify_io_t,
		auto& m,
		auto& h,
		std::derived_from<async_connector_base> auto& c,
		S& s,
		auto const& a,
		auto&& status)
		//requires requires { impl_type::notify(m, h, c, s, status); }
	{
		return S::notify(
			m,
			h,
			c,
			s,
			a,
			vsm_forward(status));
	}

	template<std::derived_from<async_operation_base> S>
	friend void tag_invoke(
		cancel_io_t,
		auto& m,
		auto const& h,
		std::derived_from<async_connector_base> auto const& c,
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
struct async_operation;

template<multiplexer Multiplexer, object Object, operation_c Operation>
using async_operation_t = async_operation<Multiplexer, Object, Operation>;


// A default async implementation using blocking I/O
// is provided for observers with bounded runtime.
template<multiplexer Multiplexer, object Object, observer Operation>
	requires std::is_same_v<typename Operation::runtime_tag, bounded_runtime_t>
struct async_operation<Multiplexer, Object, Operation>
{
	using M = Multiplexer;
	using H = typename Object::native_type const;
	using C = async_connector_t<Multiplexer, Object> const;
	using S = async_operation_t<Multiplexer, Object, Operation>;
	using A = io_parameters_t<Object, Operation>;
	using R = io_result_t<Object, Operation, Multiplexer>;

	static io_result<R> submit(M&, H& h, C&, S&, A const& a, io_handler<M>&)
	{
		return blocking_io<Object, Operation>(h, a);
	}

	static io_result<R> notify(M&, H&, C&, S&, A const&, typename M::io_status_type&&)
	{
		vsm_unreachable();
	}

	static void cancel(M&, H const&, C const&, S&)
	{
	}
};

} // namespace allio::detail
