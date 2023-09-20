#pragma once

#include <allio/detail/io_parameters.hpp>
#include <allio/error.hpp>

#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

namespace allio::detail {

class operation_base;

class io_result
{
};

template<typename T>
struct basic_io_result : io_result
{
	T value;

	template<std::convertible_to<T> U = T>
	basic_io_result(U&& value)
		: value(vsm_forward(value))
	{
	}
};

class io_reaper
{
public:
	virtual void reap(operation_base& s, io_result&& result);
};

class operation_base
{
	io_reaper* m_reaper;

protected:
	explicit operation_base(io_reaper& reaper)
		: m_reaper(&reaper)
	{
	}

	operation_base(operation_base const&) = delete;
	operation_base& operator=(operation_base const&) = delete;

	~operation_base() = default;

	void complete(io_result&& result)
	{
		m_reaper->reap(*this, vsm_move(result));
	}
};

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

struct reap_io_t
{
	template<typename M, typename H, typename C, typename S>
	std::error_code vsm_static_operator_invoke(M& m, H& h, C& c, S& s)
		requires vsm::tag_invocable<reap_io_t, M&, H&, C&, S&>
	{
		return vsm::tag_invoke(reap_io_t(), m, h, c, s);
	}
};
inline constexpr reap_io_t reap_io = {};


template<typename M, typename H>
struct connector_impl
{
};

template<typename M, typename H, typename O>
struct operation_impl
{
};


template<typename M, typename H>
class connector : public connector_impl<M, H>
{
	using impl_type = connector_impl<M, H>;
	static_assert(std::is_default_constructible_v<impl_type>);
	friend class impl_type;
};

template<typename M, typename H>
using connector_t = connector<M, H>;

template<typename M, typename H, typename O>
class operation
	: public operation_base
	, public operation_impl<M, H, O>
{
	using impl_type = operation_impl<M, H, O>;
	static_assert(std::is_default_constructible_v<impl_type>);
	friend class impl_type;

	vsm_no_unique_address io_parameters<O> args;

public:
	explicit operation(io_reaper& reaper, io_parameters<O> const& args)
		: operation_base(reaper)
		, args(args)
	{
	}

private:
	using C = connector_t<M, H>;
	using S = operation;

	friend vsm::result<void> tag_invoke(submit_io_t, M& m, H const& h, C const& c, S& s)
	{
		return operation_impl<M, H, O>::submit(m, h, c, s);
	}

	friend void tag_invoke(cancel_io_t, M& m, H const& h, C const& c, S& s, error_handler* const e)
	{
		return operation_impl<M, H, O>::cancel(m, h, c, s, e);
	}
};

template<typename M, typename H, typename O>
using operation_t = operation<M, H, O>;

} // namespace allio::detail
