#pragma once

#include <allio/detail/io_parameters.hpp>
#include <allio/error.hpp>

#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

namespace allio::detail {

class io_listener
{
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


struct create_io_t
{
	template<typename M, typename H, typename S>
	vsm::result<void> vsm_static_operator_invoke(M& m, H& h, S& s, io_listener* const l)
		requires vsm::tag_invocable<create_io_t, M&, H&, S&, io_listener*>
	{
		return vsm::tag_invoke(create_io_t(), m, h, s, l);
	}
};
inline constexpr create_io_t create_io = {};

struct submit_io_t
{
	template<typename M, typename H, typename C, typename S>
	vsm::result<void> vsm_static_operator_invoke(M& m, H& h, C& c, S& s)
		requires vsm::tag_invocable<submit_io_t, M&, H&, C&, S&>
	{
		return vsm::tag_invoke(submit_io_t(), m, h, c, s);
	}
};
inline constexpr submit_io_t submit_io = {};

struct cancel_io_t
{
	template<typename M, typename H, typename C, typename S>
	void vsm_static_operator_invoke(M& m, H& h, C& c, S& s, error_handler* const e = nullptr)
		requires vsm::tag_invocable<cancel_io_t, M&, H&, C&, S&, error_handler*>
	{
		return vsm::tag_invoke(cancel_io_t(), m, h, c, s, e);
	}
};
inline constexpr cancel_io_t cancel_io = {};


template<typename M, typename H>
struct handle_impl
{
};

template<typename M, typename H, typename O>
struct operation_impl
{
};


template<typename M, typename H>
struct handle_state : handle_impl<M, H>
{
	friend vsm::result<void> tag_invoke(attach_handle_t, M& m, H const& h, handle_state& s)
	{
		return s.handle_impl<M, H>::attach(m, h);
	}

	friend void tag_invoke(detach_handle_t, M& m, H const& h, handle_state& s, error_handler* const e)
	{
		s.handle_impl<M, H>::detach(m, h, e);
	}
};

template<typename M, typename H, typename O>
struct operation_state : operation_impl<M, H, O>
{
	vsm_no_unique_address io_parameters_with_result<O> args;

	friend vsm::result<void> tag_invoke(submit_io_t, M& m, H const& h, operation_state& s)
	{
		return s.operation_impl<M, H, O>::submit(m, h);
	}

	friend void tag_invoke(cancel_io_t, M& m, H const& h, operation_state& s, error_handler* const e)
	{
		s.operation_impl<M, H, O>::cancel(m, h, e);
	}
};

} // namespace allio::detail
