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
struct io_cancelled : basic_io_result_value<E, io_cancelled<E>>
{
	using basic_io_result_value<E, io_cancelled<E>>::basic_io_result_value;
};

template<typename E>
io_cancelled(E) -> io_cancelled<E>;

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

	static constexpr unsigned char status_pending         = 1;
	static constexpr unsigned char status_cancelled       = 2;

	unsigned char m_status = 0;

public:
	using base::value_type;

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

	template<any_cvref_of_template<io_cancelled> Cancelled>
		requires std::is_constructible_v<E, unexpected_value_t<Cancelled>>
	explicit(!std::is_convertible_v<unexpected_value_t<Cancelled>, E>)
	io_result(Cancelled&& cancelled)
		: base(vsm::result_error, cancelled.value())
		, m_status(status_cancelled)
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

	[[nodiscard]] bool is_cancelled() const
	{
		return m_status == status_cancelled;
	}


	using base::has_value;

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

#if 0
#include <vsm/assert.h>
#include <vsm/concepts.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/utility.hpp>

#include <cstdint>

namespace allio::detail {

namespace io_result_state
{
	inline constexpr uint8_t has_value              = 1 << 0;
	inline constexpr uint8_t pending                = 1 << 1;
	inline constexpr uint8_t cancelled              = 1 << 2;
}

template<typename E>
class io_pending
{
	E m_error;

public:
	explicit io_pending(auto&& error)
		: m_error(vsm_forward(error))
	{
	}

	auto&& error(this auto&& self)
	{
		return vsm_forward(self).m_error;
	}
};

template<typename E>
io_pending(E) -> io_pending<E>;

template<typename E>
class io_cancelled
{
	E m_error;

public:
	explicit io_cancelled(auto&& error)
		: m_error(vsm_forward(error))
	{
	}

	auto&& error(this auto&& self)
	{
		return vsm_forward(self).m_error;
	}
};

template<typename E>
io_cancelled(E) -> io_cancelled<E>;

template<template<typename> typename Template, typename T>
T _unexpected_value(Template<T> const&);

template<typename T>
using unexpected_value_t = vsm::copy_cvref_t<T, decltype(_unexpected_value(std::declval<T const&>()))>;

template<template<typename...> typename Template, typename... Args>
void _any_cvref_of_template(Template<Args...> const&);

template<typename T, template<typename...> typename Template>
concept any_cvref_of_template = requires (T const& t) { _any_cvref_of_template<Template>(t); };

template<typename T>
concept non_trivial_move =
	std::is_move_constructible_v<T> &&
	std::is_move_assignable_v<T> &&
	!std::is_trivially_copyable_v<T>;

template<typename T>
concept non_trivial_copy =
	std::is_copy_constructible_v<T> &&
	std::is_copy_assignable_v<T> &&
	!std::is_trivially_copyable_v<T>;

template<typename T>
concept noexcept_move =
	std::is_nothrow_move_constructible_v<T> &&
	std::is_nothrow_move_assignable_v<T>;

template<typename T>
concept noexcept_copy =
	std::is_nothrow_copy_constructible_v<T> &&
	std::is_nothrow_copy_assignable_v<T>;

template<typename T, typename E>
class io_result_base;

template<typename T, typename R>
concept other_io_result = vsm::no_cvref_of<T, R> && any_cvref_of_template<T, io_result_base>;

template<typename T1, typename E1, typename R2>
inline constexpr bool constructible =
	std::is_constructible_v<
		T1,
		vsm::copy_cvref_t<R2, typename std::remove_cvref_t<R2>::value_type>> &&
	std::is_constructible_v<
		E1,
		vsm::copy_cvref_t<R2, typename std::remove_cvref_t<R2>::error_type>>;

template<typename E1, typename R2>
inline constexpr bool constructible<void, E1, R2> =
	std::is_void_v<typename std::remove_cvref_t<R2>::value_type> &&
	std::is_constructible_v<
		E1,
		vsm::copy_cvref_t<R2, typename std::remove_cvref_t<R2>::error_type>>;

template<typename T1, typename E1, typename R2>
inline constexpr bool convertible =
	std::is_convertible_v<
		vsm::copy_cvref_t<R2, typename std::remove_cvref_t<R2>::value_type>,
		T1> &&
	std::is_convertible_v<
		vsm::copy_cvref_t<R2, typename std::remove_cvref_t<R2>::error_type>,
		E1>;

template<typename E1, typename R2>
inline constexpr bool convertible<void, E1, R2> =
	std::is_void_v<typename std::remove_cvref_t<R2>::value_type> &&
	std::is_convertible_v<
		vsm::copy_cvref_t<R2, typename std::remove_cvref_t<R2>::error_type>,
		E1>;

struct io_result_void
{
	io_result_void() = default;
	io_result_void(vsm::no_cvref_of<io_result_void> auto&&) = delete;
	io_result_void& operator=(vsm::no_cvref_of<io_result_void> auto&&) = delete;
};

template<typename T, typename E>
class io_result_base
{
	using V = vsm::select_t<std::is_void_v<T>, io_result_void, T>;

protected:
	union
	{
		struct {} m_dummy;
		V m_value;
		E m_error;
	};
	uint8_t m_state;

public:
	using value_type = T;
	using error_type = E;


	io_result_base()
		requires std::is_default_constructible_v<V>
		: m_value{}
		, m_state(io_result_state::has_value)
	{
	}

	template<vsm::no_cvref_of<io_result_base> U = T>
		requires std::is_constructible_v<T, U>
	explicit(!std::is_convertible_v<U, T>)
	io_result_base(U&& value)
		: m_value(vsm_forward(value))
		, m_state(io_result_state::has_value)
	{
	}

	template<any_cvref_of_template<vsm::unexpected> U>
		requires std::is_constructible_v<E, unexpected_value_t<U>>
	explicit(!std::is_convertible_v<unexpected_value_t<U>, E>)
	io_result_base(U&& unexpected)
		: m_error(vsm_forward(unexpected).error())
		, m_state(0)
	{
	}

	template<any_cvref_of_template<io_pending> U>
		requires std::is_constructible_v<E, unexpected_value_t<U>>
	explicit(!std::is_convertible_v<unexpected_value_t<U>, E>)
	io_result_base(U&& pending)
		: m_error(vsm_forward(pending).error())
		, m_state(io_result_state::pending)
	{
	}

	template<any_cvref_of_template<io_cancelled> U>
		requires std::is_constructible_v<E, unexpected_value_t<U>>
	explicit(!std::is_convertible_v<unexpected_value_t<U>, E>)
	io_result_base(U&& cancelled)
		: m_error(vsm_forward(cancelled).error())
		, m_state(io_result_state::cancelled)
	{
	}

	template<typename... Args>
		requires std::is_constructible_v<V, Args...>
	explicit io_result_base(std::in_place_t, Args&&... args)
		: m_value(vsm_forward(args)...)
		, m_state(io_result_state::has_value)
	{
	}

	template<typename... Args>
		requires std::is_constructible_v<E, Args...>
	explicit io_result_base(std::unexpect_t, Args&&... args)
		: m_error(vsm_forward(args)...)
		, m_state(0)
	{
	}

	template<any_cvref_of_template<vsm::expected> U>
		requires constructible<T, E, U>
	explicit(!convertible<T, E, U>)
	io_result_base(U&& other)
		: m_dummy{}
	{
		if (other)
		{
			if constexpr (std::is_void_v<T>)
			{
				new (&m_value) V{};
			}
			else
			{
				new (&m_value) V(vsm_forward(other).value());
			}
			m_state = io_result_state::has_value;
		}
		else
		{
			new (&m_error) E(vsm_forward(other).error());
			m_state = 0;
		}
	}

	template<other_io_result<io_result_base> U>
		requires constructible<T, E, U>
	explicit(!convertible<T, E, U>)
	io_result_base(U&& other)
		: m_dummy{}
		, m_state(other.m_state)
	{
		construct(vsm_forward(other));
	}

	io_result_base(io_result_base&& other) = default;
	io_result_base(io_result_base&& other)
		noexcept(noexcept_move<T> && noexcept_move<E>)
		requires non_trivial_move<T> || non_trivial_move<E>
		: m_dummy{}
		, m_state(other.m_state)
	{
		construct(vsm_move(other));
	}

	io_result_base(io_result_base const& other) = default;
	io_result_base(io_result_base const& other)
		noexcept(noexcept_copy<T> && noexcept_copy<E>)
		requires non_trivial_copy<T> || non_trivial_copy<E>
		: m_dummy{}
		, m_state(other.m_state)
	{
		construct(other);
	}


	template<vsm::no_cvref_of<io_result_base> U = T>
		requires std::is_convertible_v<U, T>
	io_result_base& operator=(U&& value) &
	{
		assign_value(vsm_forward(value));
		return *this;
	}
	
	template<any_cvref_of_template<vsm::unexpected> U>
		requires std::is_convertible_v<unexpected_value_t<U>, E>
	io_result_base& operator=(U&& unexpected) &
	{
		assign_error(vsm_forward(unexpected).error());
		m_state = 0;
		return *this;
	}
	
	template<any_cvref_of_template<io_pending> U>
		requires std::is_convertible_v<unexpected_value_t<U>, E>
	io_result_base& operator=(U&& pending) &
	{
		assign_error(vsm_forward(pending).error());
		m_state = io_result_state::pending;
		return *this;
	}
	
	template<any_cvref_of_template<io_cancelled> U>
		requires std::is_convertible_v<unexpected_value_t<U>, E>
	io_result_base& operator=(U&& cancelled) &
	{
		assign_error(vsm_forward(cancelled).error());
		m_state = io_result_state::cancelled;
		return *this;
	}

	template<any_cvref_of_template<vsm::expected> U>
		requires convertible<T, E, U>
	io_result_base& operator=(U&& other) &
	{
		if (m_state & io_result_state::has_value)
		{
			if (other)
			{
				if constexpr (std::is_void_v<T>)
				{
				}
				else
				{
					m_value = vsm_forward(other).value();
				}
				m_state = io_result_state::has_value;
			}
			else
			{
				new (&m_error) E(vsm_forward(other).error());
				m_state = 0;
			}
		}
		else
		{
			if (other)
			{
				if constexpr (std::is_void_v<T>)
				{
					new (&m_value) V{};
				}
				else
				{
					new (&m_value) V(vsm_forward(other).value());
				}
				m_state = io_result_state::has_value;
			}
			else
			{
				m_error = vsm_forward(other).error();
				m_state = 0;
			}
		}
	}

	template<other_io_result<io_result_base> U>
		requires convertible<T, E, U>
	io_result_base& operator=(U&& other) &
	{
		assign(vsm_forward(other));
		return *this;
	}

	io_result_base& operator=(io_result_base&& other) & = default;
	io_result_base& operator=(io_result_base&& other) &
		noexcept(noexcept_move<V> && noexcept_move<E>)
		requires non_trivial_move<V> || non_trivial_move<E>
	{
		assign(vsm_move(other));
		return *this;
	}

	io_result_base& operator=(io_result_base const& other) & = default;
	io_result_base& operator=(io_result_base const& other) &
		noexcept(noexcept_copy<V> && noexcept_copy<E>)
		requires non_trivial_copy<V> || non_trivial_copy<E>
	{
		assign(vsm_move(other));
		return *this;
	}
	

	~io_result_base() = default;
	~io_result_base()
		requires (!std::is_trivially_destructible_v<V> || !std::is_trivially_destructible_v<E>)
	{
		if (m_state & io_result_state::has_value)
		{
			m_value.~V();
		}
		else
		{
			m_error.~E();
		}
	}


	[[nodiscard]] bool has_value() const
	{
		return (m_state & io_result_state::has_value) != 0;
	}

	[[nodiscard]] bool has_error() const
	{
		return (m_state & io_result_state::has_value) == 0;
	}

	[[nodiscard]] bool is_pending() const
	{
		return (m_state & io_result_state::pending) != 0;
	}

	[[nodiscard]] bool is_cancelled() const
	{
		return (m_state & io_result_state::cancelled) != 0;
	}

	[[nodiscard]] operator bool() const
	{
		return (m_state & io_result_state::has_value) != 0;
	}


	[[nodiscard]] auto&& error(this auto&& self)
	{
		vsm_assert((self.m_state & io_result_state::has_value) == 0);
		return vsm_forward(self).m_error;
	}

	template<typename Self>
	[[nodiscard]] vsm::expected<T, E> discard_flags(this Self&& self)
	{
		if (self.m_state & io_result_state::has_value)
		{
			if constexpr (std::is_void_v<T>)
			{
				return {};
			}
			else
			{
				return vsm::expected<T, E>(vsm::result_value, vsm_forward(self).m_value);
			}
		}
		else
		{
			return vsm::expected<T, E>(vsm::result_error, vsm_forward(self).m_error);
		}
	}

private:
	void construct(auto&& other)
	{
		if (other.m_state & io_result_state::has_value)
		{
			new (&m_value) T(vsm_forward(other).m_value);
		}
		else
		{
			new (&m_error) E(vsm_forward(other).m_error);
		}
	}

	void assign(auto&& other)
	{
		if (m_state & io_result_state::has_value)
		{
			if (other.m_state & io_result_state::has_value)
			{
				m_value = vsm_forward(other).m_value;
			}
			else
			{
				m_value.~V();
				new (&m_error) E(vsm_forward(other).m_error);
			}
		}
		else
		{
			if (other.m_state & io_result_state::has_value)
			{
				m_error.~E();
				new (&m_value) V(vsm_forward(other).m_value);
			}
			else
			{
				m_error = vsm_forward(other).m_error;
			}
		}
		m_state = other.m_state;
	}

	void assign_value(auto&& value)
	{
		if (m_state & io_result_state::has_value)
		{
			m_value = vsm_forward(value);
		}
		else
		{
			m_error.~E();
			new (&m_value) V(vsm_forward(value));
		}
	}

	void assign_error(auto&& error)
	{
		if (m_state & io_result_state::has_value)
		{
			m_value.~V();
			new (&m_error) E(vsm_forward(error));
		}
		else
		{
			m_error = vsm_forward(error);
		}
	}
};

template<typename T, typename E = std::error_code>
class io_result final : public io_result_base<T, E>
{
	using base = io_result_base<T, E>;

public:
	using base::base;

	[[nodiscard]] auto&& value(this auto&& self)
	{
		vsm_assert((self.m_state & io_result_state::has_value) != 0);
		return vsm_forward(self).m_value;
	}

	[[nodiscard]] auto&& operator*(this auto&& self)
	{
		vsm_assert((self.m_state & io_result_state::has_value) != 0);
		return vsm_forward(self).m_value;
	}

	[[nodiscard]] auto*& operator->(this auto&& self)
	{
		vsm_assert((self.m_state & io_result_state::has_value) != 0);
		return &self.m_value;
	}
};

template<typename E>
class io_result<void, E> final : public io_result_base<void, E>
{
	using base = io_result_base<void, E>;

public:
	using base::base;

	void value() const
	{
		vsm_assert((base::m_state & io_result_state::has_value) != 0);
	}

	void operator*() const
	{
		vsm_assert((base::m_state & io_result_state::has_value) != 0);
	}
};

} // namespace allio::detail
#endif
