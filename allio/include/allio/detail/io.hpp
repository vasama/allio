#pragma once

#include <allio/detail/io_parameters.hpp>
#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

#include <memory>
#include <optional>

namespace allio::detail {

struct blocking_io_t
{
	template<typename H, typename O>
	auto vsm_static_operator_invoke(H& handle, io_parameters_t<O> const& args)
		requires vsm::tag_invocable<blocking_io_t, H&, io_parameters_t<O> const&>
	{
		return vsm::tag_invoke(blocking_io_t(), handle, args);
	}
};
inline constexpr blocking_io_t blocking_io = {};


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
	template<typename M, typename H, typename S>
	vsm::result<void> vsm_static_operator_invoke(M& m, H const& h, S& s)
		requires vsm::tag_invocable<attach_handle_t, M&, H const&, S&>
	{
		return vsm::tag_invoke(attach_handle_t(), m, h, s);
	}
};
inline constexpr attach_handle_t attach_handle = {};

struct detach_handle_t
{
	template<typename M, typename H, typename S>
	void vsm_static_operator_invoke(M& m, H const& h, S& s)
		requires vsm::tag_invocable<detach_handle_t, M const&, H const&, S&>
	{
		return vsm::tag_invoke(detach_handle_t(), m, h, s);
	}
};
inline constexpr detach_handle_t detach_handle = {};


struct submit_io_t
{
	template<typename H, typename S>
	vsm::result<io_result> vsm_static_operator_invoke(H& h, S& s)
		requires vsm::tag_invocable<submit_io_t, H&, S&>
	{
		return vsm::tag_invoke(submit_io_t(), h, s);
	}

	template<typename M, typename H, typename C, typename S>
	vsm::result<io_result> vsm_static_operator_invoke(M& m, H& h, C& c, S& s)
		requires vsm::tag_invocable<submit_io_t, M&, H&, C&, S&>
	{
		return vsm::tag_invoke(submit_io_t(), m, h, c, s);
	}
};
inline constexpr submit_io_t submit_io = {};

struct notify_io_t
{
	template<typename H, typename S>
	vsm::result<io_result> vsm_static_operator_invoke(H& h, S& s, io_status const status)
		requires vsm::tag_invocable<notify_io_t, H&, S&, io_status>
	{
		return vsm::tag_invoke(notify_io_t(), h, s, status);
	}

	template<typename M, typename H, typename C, typename S>
	vsm::result<io_result> vsm_static_operator_invoke(M& m, H& h, C& c, S& s, io_status const status)
		requires vsm::tag_invocable<notify_io_t, M&, H&, C&, S&, io_status>
	{
		return vsm::tag_invoke(notify_io_t(), m, h, c, s, status);
	}
};
inline constexpr notify_io_t notify_io = {};

struct cancel_io_t
{
	template<typename M, typename H, typename C, typename S>
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
	vsm::result<void> vsm_static_operator_invoke(Multiplexer& m, Args&&... args)
		requires vsm::tag_invocable<poll_io_t, Multiplexer&, Args&&...>
	{
		return vsm::tag_invoke(poll_io_t(), m, vsm_forward(args)...);
	}
};
inline constexpr poll_io_t poll_io = {};


template<typename M, typename H>
struct connector_impl
{
};

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
struct operation_impl
{
};

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
	using C = connector_t<M, H>;
	using S = operation;

	friend vsm::result<io_result> tag_invoke(submit_io_t, M& m, H const& h, C const& c, S& s)
	{
		return operation_impl<M, H, O>::submit(m, h, c, s);
	}

	friend vsm::result<io_result> tag_invoke(notify_io_t, M& m, H const& h, C const& c, S& s, io_status const status)
	{
		return operation_impl<M, H, O>::notify(m, h, c, s, status);
	}

	friend void tag_invoke(cancel_io_t, M& m, H const& h, C const& c, S& s)
	{
		return operation_impl<M, H, O>::cancel(m, h, c, s);
	}
};

template<typename M, typename H, typename O>
using operation_t = operation<M, H, O>;


template<typename M>
using multiplexer_t = typename std::pointer_traits<M>::element_type;

} // namespace allio::detail
