#pragma once

#include <allio/detail/io_parameters.hpp>
#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

#include <optional>

namespace allio::detail {

struct blocking_io_t
{
};


//TODO: Rename this to io_result
using submit_result = std::optional<std::error_code>;

class operation_base;


class io_status;

template<typename T>
io_status* wrap_io_status(T& object)
{
	return reinterpret_cast<io_status*>(&object);
}

template<typename T>
T& unwrap_io_status(io_status* const status)
{
	return *reinterpret_cast<T*>(status);
}


class io_callback
{
public:
	virtual void notify(operation_base& s, io_status* status) noexcept = 0;

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

	void notify(io_status* const status)
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
	void vsm_static_operator_invoke(M& m, H const& h, S& s, error_handler* const e = nullptr)
		requires vsm::tag_invocable<detach_handle_t, M const&, H const&, S&, error_handler*>
	{
		return vsm::tag_invoke(detach_handle_t(), m, h, s, e);
	}
};
inline constexpr detach_handle_t detach_handle = {};


struct submit_io_t
{
	template<typename H, typename S>
	vsm::result<submit_result> vsm_static_operator_invoke(H& h, S& s)
		requires vsm::tag_invocable<submit_io_t, H&, S&>
	{
		return vsm::tag_invoke(submit_io_t(), h, s);
	}

	template<typename M, typename H, typename C, typename S>
	vsm::result<submit_result> vsm_static_operator_invoke(M& m, H& h, C& c, S& s)
		requires vsm::tag_invocable<submit_io_t, M&, H&, C&, S&>
	{
		return vsm::tag_invoke(submit_io_t(), m, h, c, s);
	}
};
inline constexpr submit_io_t submit_io = {};

struct notify_io_t
{
	template<typename H, typename S>
	vsm::result<submit_result> vsm_static_operator_invoke(H& h, S& s, io_status* const status)
		requires vsm::tag_invocable<notify_io_t, H&, S&, io_status*>
	{
		return vsm::tag_invoke(notify_io_t(), h, s, status);
	}

	template<typename M, typename H, typename C, typename S>
	vsm::result<submit_result> vsm_static_operator_invoke(M& m, H& h, C& c, S& s, io_status* const status)
		requires vsm::tag_invocable<notify_io_t, M&, H&, C&, S&, io_status*>
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
	friend class impl_type;
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
	friend class impl_type;

	vsm_no_unique_address io_parameters<O> args;

public:
	explicit operation(io_callback& callback, io_parameters<O> const& args)
		: M::operation_type(callback)
		, args(args)
	{
	}

private:
	using C = connector_t<M, H>;
	using S = operation;

	friend vsm::result<submit_result> tag_invoke(submit_io_t, M& m, H const& h, C const& c, S& s)
	{
		return operation_impl<M, H, O>::submit(m, h, c, s);
	}

	friend vsm::result<submit_result> tag_invoke(notify_io_t, M& m, H const& h, C const& c, S& s, io_status* const status)
	{
		return operation_impl<M, H, O>::notify(m, h, c, s, status);
	}

#if 0
	friend std::error_code tag_invoke(reap_io_t, M& m, H const& h, C const& c, S& s, io_result&& r)
	{
		return operation_impl<M, H, O>::reap(m, h, c, s, vsm_move(r));
	}
#endif

	friend void tag_invoke(cancel_io_t, M& m, H const& h, C const& c, S& s, error_handler* const e)
	{
		return operation_impl<M, H, O>::cancel(m, h, c, s, e);
	}
};

template<typename M, typename H, typename O>
using operation_t = operation<M, H, O>;

} // namespace allio::detail
