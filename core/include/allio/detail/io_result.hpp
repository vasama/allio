#pragma once

#include <allio/detail/concepts.hpp>

#include <vsm/detail/categories.hpp>
#include <vsm/result.hpp>
#include <vsm/utility.hpp>

namespace allio::detail {

template<template<typename> typename Template, typename T>
T _unexpected_value(Template<T> const&);

template<typename T>
using unexpected_value_t = vsm::copy_cvref_t<T, decltype(_unexpected_value(std::declval<T const&>()))>;

template<typename T, typename Tag>
class basic_io_result_value
{
	T m_value;

public:
	using value_type = T;

	explicit basic_io_result_value(auto&& value)
		: m_value(vsm_forward(value))
	{
	}

#if __cpp_explicit_this_parameter
	auto&& value(this auto&& self)
	{
		return vsm_forward(self).m_value;
	}
#else
#	define allio_detail_x(C) \
	T C value() C \
	{ \
		return static_cast<T C>(m_value); \
	} \

	vsm_detail_reference_categories(allio_detail_x)
#	undef allio_detail_x
#endif
};

template<typename E>
struct io_pending : basic_io_result_value<E, io_pending<E>>
{
	using basic_io_result_value<E, io_pending<E>>::basic_io_result_value;
};

template<typename E>
io_pending(E) -> io_pending<E>;

template<typename E>
struct io_canceled : basic_io_result_value<E, io_canceled<E>>
{
	using basic_io_result_value<E, io_canceled<E>>::basic_io_result_value;
};

template<typename E>
io_canceled(E) -> io_canceled<E>;

template<typename E>
class io_unexpected : public basic_io_result_value<E, void>
{
	unsigned char m_status;

public:
	io_unexpected(std::convertible_to<E> auto&& error, unsigned char const status)
		: basic_io_result_value<E, void>(vsm_forward(error))
		, m_status(status)
	{
	}

	unsigned char status() const
	{
		return m_status;
	}
};

template<typename T, typename E = std::error_code>
class io_result : vsm::result<T, E>
{
	using base = vsm::result<T, E>;

	static constexpr unsigned char status_pending           = 1;
	static constexpr unsigned char status_canceled          = 2;

	unsigned char m_status = 0;

public:
	using typename base::value_type;

	using base::base;

	template<vsm::any_cvref_of<base> Result>
	io_result(Result&& result)
		: base(vsm_forward(result))
	{
	}

	template<any_cvref_of_template<io_pending> Pending>
		requires std::is_constructible_v<E, unexpected_value_t<Pending>>
	explicit(!std::is_convertible_v<unexpected_value_t<Pending>, E>)
	io_result(Pending&& pending)
		: base(vsm::result_error, pending.value())
		, m_status(status_pending)
	{
	}

	template<any_cvref_of_template<io_canceled> Canceled>
		requires std::is_constructible_v<E, unexpected_value_t<Canceled>>
	explicit(!std::is_convertible_v<unexpected_value_t<Canceled>, E>)
	io_result(Canceled&& canceled)
		: base(vsm::result_error, canceled.value())
		, m_status(status_canceled)
	{
	}

	template<any_cvref_of_template<io_unexpected> Unexpected>
		requires std::is_constructible_v<E, unexpected_value_t<Unexpected>>
	explicit(!std::is_convertible_v<unexpected_value_t<Unexpected>, E>)
		io_result(Unexpected&& unexpected)
		: base(vsm::result_error, unexpected.value())
		, m_status(unexpected.status())
	{
	}


	[[nodiscard]] bool is_pending() const
	{
		return m_status == status_pending;
	}

	[[nodiscard]] bool is_canceled() const
	{
		return m_status == status_canceled;
	}


	using base::has_value;
	using base::operator bool;

	using base::value;
	using base::error;

	using base::operator*;

private:
	friend bool tag_invoke(vsm::has_error_t, io_result const& r)
	{
		return !r.has_value();
	}

	template<vsm::any_cvref_of<io_result> R>
	friend io_unexpected<vsm::copy_cvref_t<R&&, E>> tag_invoke(vsm::propagate_error_t, R&& r)
	{
		return io_unexpected<vsm::copy_cvref_t<R&&, E>>(vsm_forward(r).error(), r.m_status);
	}
};

} // namespace allio::detail
