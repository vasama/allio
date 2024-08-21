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


template<handle Handle, operation_c Operation>
typename Operation::template result_type_template<Handle> _io_result(int);

template<handle Handle, operation_c Operation>
typename Operation::result_type _io_result(...);

template<handle Handle, operation_c Operation>
using io_result_t = decltype(detail::_io_result<Handle, Operation>(0));


template<typename Object, operation_c Operation>
typename Operation::template params_type_template<Object> _io_params(int);

template<typename Object, operation_c Operation>
typename Operation::params_type _io_params(...);

template<typename Object, operation_c Operation>
using io_parameters_t = decltype(detail::_io_params<Object, Operation>(0));


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


template<operation_c Operation>
struct blocking_io_t
{
	template<object Object>
	[[nodiscard]] vsm_static_operator auto operator()(
		handle_const_t<Operation, native_handle<Object>>& h,
		io_parameters_t<Object, Operation> const& a) vsm_static_operator_const
	{
		if constexpr (requires { Object::template blocking_io<Operation>(h, a); })
		{
			return Object::template blocking_io<Operation>(h, a);
		}
		else
		{
			return Operation::template blocking_io<Object>(h, a);
		}
	}

	template<handle Handle>
		requires vsm::tag_invocable<
			blocking_io_t,
			Handle&,
			io_parameters_t<typename Handle::object_type, Operation> const&>
	[[nodiscard]] vsm_static_operator auto operator()(
		Handle& h,
		io_parameters_t<typename Handle::object_type, Operation> const& a) vsm_static_operator_const
	{
		return vsm::tag_invoke(blocking_io_t(), h, a);
	}
};

template<operation_c Operation>
inline constexpr blocking_io_t<Operation> blocking_io = {};

#if 0
template<consumer Operation>
struct consume_t
{
	template<handle Handle>
	[[nodiscard]] vsm_static_operator auto operator()(
		Handle& h,
		io_parameters_t<typename Handle::object_type, Operation> const& a) vsm_static_operator_const
	{
		using object_type = typename Handle::object_type;

		native_handle<object_type> local_h = h.native();
		auto r = detail::blocking_io<Operation>(local_h);

		if (r)
		{
			[[maybe_unused]] std::same_as<native_handle<object_type>> auto const _ = h.release();
		}

		return r;
	}
};

template<consumer Operation>
inline constexpr consume_t<Operation> consume = {};
#endif

#if 0
template<handle Handle, producer Operation>
struct produce_t
{
	[[nodiscard]] vsm_static_operator vsm::result<Handle> operator()(
		io_parameters_t<typename Handle::object_type, Operation> const& a) vsm_static_operator_const
	{
		using object_type = typename Handle::object_type;

		native_handle<object_type> h = {};
		vsm_try_void(detail::blocking_io<Operation>(h));

		auto const r = Handle::adopt(h);

		if (!r)
		{
			//TODO: Use a destructor instead. Ideally basic_detached_handle. This requires some
			//      changes to the include order.
			vsm_verify(detail::blocking_io<close_t>(h));
		}

		return r;
	}
};

template<handle Handle, producer Operation>
inline constexpr produce_t<Handle, Operation> produce = {};
#endif

template<observer Operation>
struct observe_t
{
	template<object Object>
	[[nodiscard]] vsm_static_operator auto operator()(
		native_handle<Object> const& h,
		io_parameters_t<Object, Operation> const& a) vsm_static_operator_const
	{
		return detail::blocking_io<Operation>(h, a);
	}

	template<handle Handle>
	[[nodiscard]] vsm_static_operator auto operator()(
		Handle const& h,
		io_parameters_t<typename Handle::object_type, Operation> const& a) vsm_static_operator_const
		requires requires { detail::blocking_io<Operation>(h.native(), a); }
	{
		return detail::blocking_io<Operation>(h.native(), a);
	}
};

template<observer Operation>
inline constexpr observe_t<Operation> observe = {};


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

protected:
	basic_io_handler(basic_io_handler const&) = default;
	basic_io_handler& operator=(basic_io_handler const&) = default;
	~basic_io_handler() = default;
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
	[[nodiscard]] vsm_static_operator vsm::result<void> operator()(
		M& m,
		H const& h,
		C& c) vsm_static_operator_const
	{
		return C::attach(m, h, c);
	}
};
inline constexpr attach_handle_t attach_handle = {};

struct detach_handle_t
{
	template<typename M, typename H, typename C>
	[[nodiscard]] vsm_static_operator vsm::result<void> operator()(
		M& m,
		H const& h,
		C& c) vsm_static_operator_const
	{
		return C::detach(m, h, c);
	}
};
inline constexpr detach_handle_t detach_handle = {};

template<typename To>
struct rebind_handle_t
{
	template<typename From, typename... Args>
	[[nodiscard]] vsm_static_operator vsm::result<To> operator()(
		From&& from,
		Args&&... args) vsm_static_operator_const
	{
		if constexpr (vsm::any_cvref_of<From, To>)
		{
			return vsm_forward(from);
		}
		else
		{
			return rebind_traits<std::remove_cvref_t<From>, To>::rebind(
				vsm_forward(from),
				vsm_forward(args)...);
		}
	}
};
template<typename To>
inline constexpr rebind_handle_t<To> rebind_handle = {};


struct submit_io_t
{
	template<typename H, typename S, typename A, typename Handler>
	[[nodiscard]] vsm_static_operator auto operator()(
		H& h,
		S& s,
		A const& a,
		Handler& handler) vsm_static_operator_const
	{
		return handle_traits<std::remove_cv_t<H>>::submit_io(h, s, a, handler);
	}

	template<typename M, typename H, typename C, typename S, typename A, typename Handler>
	[[nodiscard]] vsm_static_operator auto operator()(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		Handler& handler) vsm_static_operator_const
	{
		return S::submit(m, h, c, s, a, handler);
	}
};
inline constexpr submit_io_t submit_io = {};

struct notify_io_t
{
	template<typename H, typename S, typename A, typename Status>
	[[nodiscard]] vsm_static_operator auto operator()(
		H& h,
		S& s,
		A const& a,
		Status&& status) vsm_static_operator_const
	{
		return handle_traits<std::remove_cv_t<H>>::notify_io(
			h,
			s,
			a,
			static_cast<Status&&>(status));
	}

	template<typename M, typename H, typename C, typename S, typename A, typename Status>
	[[nodiscard]] vsm_static_operator auto operator()(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		Status&& status) vsm_static_operator_const
	{
		return S::notify(m, h, c, s, a, static_cast<Status&&>(status));
	}
};
inline constexpr notify_io_t notify_io = {};

struct cancel_io_t
{
	template<typename H, typename S>
	[[nodiscard]] vsm_static_operator void operator()(
		H& h,
		S& s) vsm_static_operator_const
	{
		return handle_traits<std::remove_cv_t<H>>::cancel_io(h, s);
	}

	template<typename M, typename H, typename C, typename S>
	[[nodiscard]] vsm_static_operator void operator()(
		M& m,
		H& h,
		C& c,
		S& s) vsm_static_operator_const
	{
		return S::cancel(m, h, c, s);
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
	[[deprecated]] friend auto tag_invoke(
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
	[[deprecated]] friend auto tag_invoke(
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
	[[deprecated]] friend void tag_invoke(
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


//TODO: Move this somewhere else. Maybe a handle forward declaration header.
template<object Object, multiplexer_handle_for<Object> MultiplexerHandle>
class basic_attached_handle;

// A default async implementation using blocking I/O is provided for observers with bounded runtime.
template<multiplexer Multiplexer, object Object, observer Operation>
	requires std::is_same_v<typename Operation::runtime_tag, bounded_runtime_t>
struct async_operation<Multiplexer, Object, Operation>
{
	using M = Multiplexer;
	using H = typename Object::native_type const;
	using C = async_connector_t<Multiplexer, Object> const;
	using S = async_operation_t<Multiplexer, Object, Operation>;
	using A = io_parameters_t<Object, Operation>;
	using R = io_result_t<
		basic_attached_handle<Object, multiplexer_handle_t<Multiplexer>>,
		Operation>;

	static io_result<R> submit(M&, H& h, C&, S&, A const& a, io_handler<M>&)
	{
		return blocking_io<Operation>(h, a);
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
