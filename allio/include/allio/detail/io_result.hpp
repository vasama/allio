#pragma once

#include <vsm/assert.h>
#include <vsm/concepts.hpp>
#include <vsm/standard.hpp>
#include <vsm/result.hpp>

#include <cstdint>

namespace allio::detail {

namespace io_result_state
{
	inline constexpr uint8_t completed              = 0b0000;
	inline constexpr uint8_t completed_partial      = 0b0010;
	inline constexpr uint8_t pending                = 0b0011;
	inline constexpr uint8_t cancelled              = 0b0100;
	inline constexpr uint8_t failure                = 0b1000;
	inline constexpr uint8_t multiplexer_busy       = 0b1001;

	inline constexpr uint8_t pending_flag           = 0b010;
}

struct io_pending_t {};
inline constexpr io_pending_t io_pending = {};

struct io_partial_t {};
inline constexpr io_partial_t io_partial = {};

struct multiplexer_busy_t {};
inline constexpr multiplexer_busy_t multiplexer_busy = {};

union io_result_error
{
	struct {} dummy;
	std::error_code error;
};

template<typename T>
class io_result2
{
	using E = std::error_code;

protected:
	union
	{
		io_result_error m_error;
		T m_value;
	};
	uint8_t m_state;

public:
	using value_type = T;
	using error_type = E;


	io_result2(io_pending_t)
		: m_error{ .dummy = {} }
		, m_state(io_result_state::pending)
	{
	}

	io_result2()
		requires std::is_default_constructible_v<T>
		: m_value{}
		, m_state(io_result_state::completed)
	{
	}

	io_result2(io_partial_t)
		requires std::is_default_constructible_v<T>
		: m_value{}
		, m_state(io_result_state::completed_partial)
	{
	}

	template<vsm::no_cvref_of<io_result2> U = T>
		requires std::is_constructible_v<T, U>
	explicit(!std::is_convertible_v<U, T>)
	io_result2(U&& value)
		: m_value(vsm_forward(value))
		, m_state(io_result_state::completed)
	{
	}

	template<vsm::no_cvref_of<io_result2> U = T>
		requires std::is_constructible_v<T, U>
	explicit(!std::is_convertible_v<U, T>)
	io_result2(io_partial_t, U&& value)
		: m_value(vsm_forward(value))
		, m_state(io_result_state::completed_partial)
	{
	}

	template<vsm::constructible_to<T> U>
	explicit(!std::is_convertible_v<U, T>)
	io_result2(vsm::result<T>&& result)
		: m_error{ .dummy = {} }
	{
		if (result)
		{
			new (&m_value) T(vsm_move(*result));
			m_state = io_result_state::completed;
		}
		else
		{
			m_error.error = result.error();
			m_state = io_result_state::failure;
		}
	}

	template<vsm::constructible_to<T> U>
	explicit(!std::is_convertible_v<U, T>)
	io_result2(vsm::result<T> const& result)
		: m_error{ .dummy = {} }
	{
		if (result)
		{
			new (&m_value) T(*result);
			m_state = io_result_state::completed;
		}
		else
		{
			m_error.error = result.error();
			m_state = io_result_state::failure;
		}
	}

	template<typename U>
		requires std::is_constructible_v<E, U const&>
	explicit(!std::is_convertible_v<U const&, E>)
	io_result2(vsm::unexpected<U> const& e)
		: m_error{ .error = e.error() }
		, m_state(io_result_state::failure)
	{
	}

	template<typename U>
		requires std::is_constructible_v<E, U const&>
	explicit(!std::is_convertible_v<U const&, E>)
	io_result2(multiplexer_busy_t, vsm::unexpected<U> const& e)
		: m_error{ .error = e.error() }
		, m_state(io_result_state::multiplexer_busy)
	{
	}

	io_result2(io_result2&& other) = default;
	io_result2(io_result2&& other)
		requires (std::is_move_constructible_v<T> && !std::is_trivially_copyable_v<T>)
		: m_error(other.m_error)
		, m_state(other.m_state)
	{
		if (has_value())
		{
			new (&m_value) T(vsm_move(other.m_value));
		}
	}

	template<vsm::not_same_as<T> U>
		requires std::is_constructible_v<T, U>
	explicit(!std::is_convertible_v<U, T>)
	io_result2(io_result2<U>&& other)
		: m_error(other.m_error)
		, m_state(other.m_state)
	{
		if (has_value())
		{
			new (&m_value) T(vsm_move(other.m_value));
		}
	}

	io_result2(io_result2 const& other) = default;
	io_result2(io_result2 const& other)
		requires (std::is_copy_constructible_v<T> && !std::is_trivially_copyable_v<T>)
		: m_error(other.m_error)
		, m_state(other.m_state)
	{
		if (has_value())
		{
			new (&m_value) T(other.m_value);
		}
	}

	template<vsm::not_same_as<T> U>
		requires std::is_constructible_v<T, U const&>
	explicit(!std::is_convertible_v<U const&, T>)
	io_result2(io_result2 const& other)
		: m_error(other.m_error)
		, m_state(other.m_state)
	{
		if (has_value())
		{
			new (&m_value) T(other.m_value);
		}
	}

	io_result2& operator=(io_result2&& other) & = default;
	io_result2& operator=(io_result2&& other) &
		requires (std::is_move_constructible_v<T> && std::is_move_assignable_v<T> && !std::is_trivially_copyable_v<T>)
	{
		if (has_value() && other.has_value())
		{
			m_value = vsm_move(other.m_value);
		}
		else
		{
			if (has_value())
			{
				m_value.~T();
				m_error = other.m_error;
			}
			else
			{
				new (&m_value) T(vsm_move(other.m_value));
			}
			m_state = other.m_state;
		}
		return *this;
	}

	template<vsm::not_same_as<T> U>
		requires std::is_convertible_v<U, T>
	io_result2& operator=(io_result2<U>&& other) &
	{
		if (has_value() && other.has_value())
		{
			m_value = vsm_move(other.m_value);
		}
		else
		{
			if (has_value())
			{
				m_value.~T();
				m_error = other.m_error;
			}
			else
			{
				new (&m_value) T(vsm_move(other.m_value));
			}
			m_state = other.m_state;
		}
		return *this;
	}

	io_result2& operator=(io_result2 const& other) & = default;
	io_result2& operator=(io_result2 const& other) &
		requires (std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T> && !std::is_trivially_copyable_v<T>)
	{
		if (has_value() && other.has_value())
		{
			m_value = other.m_value;
		}
		else if (has_value())
		{
			if (has_value())
			{
				m_value.~T();
				m_error = other.m_error;
			}
			else
			{
				new (&m_value) T(other.m_value);
			}
			m_state = other.m_state;
		}
		return *this;
	}

	template<vsm::not_same_as<T> U>
		requires std::is_convertible_v<U const&, T>
	io_result2& operator=(io_result2<U> const& other) &
	{
		if (has_value() && other.has_value())
		{
			m_value = other.m_value;
		}
		else if (has_value())
		{
			if (has_value())
			{
				m_value.~T();
				m_error = other.m_error;
			}
			else
			{
				new (&m_value) T(other.m_value);
			}
			m_state = other.m_state;
		}
		return *this;
	}

	~io_result2() = default;
	~io_result2()
		requires (!std::is_trivially_destructible_v<T>)
	{
		if (has_value())
		{
			m_value.~T();
		}
	}


	[[nodiscard]] bool has_value() const
	{
		return m_state <= io_result_state::completed_partial;
	}
	
	[[nodiscard]] bool has_error() const
	{
		return m_state >= io_result_state::pending;
	}

	[[nodiscard]] bool is_pending() const
	{
		return m_state & io_result_state::pending_flag;
	}

	[[nodiscard]] bool is_cancelled() const
	{
		return m_state == io_result_state::cancelled;
	}

	[[nodiscard]] bool is_busy_error() const
	{
		return m_state == io_result_state::multiplexer_busy;
	}

	[[nodiscard]] E const& error() const
	{
		vsm_assert(m_state >= io_result_state::failure);
		return m_error.error;
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_state <= io_result_state::completed_partial;
	}


	[[nodiscard]] T& operator*() &
	{
		vsm_assert(m_state <= io_result_state::completed_partial);
		return static_cast<T&>(m_value);
	}

	[[nodiscard]] T const& operator*() const&
	{
		vsm_assert(m_state <= io_result_state::completed_partial);
		return static_cast<T const&>(m_value);
	}

	[[nodiscard]] T&& operator*() &&
	{
		vsm_assert(m_state <= io_result_state::completed_partial);
		return static_cast<T&&>(m_value);
	}

	[[nodiscard]] T const&& operator*() const&&
	{
		vsm_assert(m_state <= io_result_state::completed_partial);
		return static_cast<T const&&>(m_value);
	}

	[[nodiscard]] T* operator->()
	{
		vsm_assert(m_state <= io_result_state::completed_partial);
		return &static_cast<T&>(m_value);
	}

	[[nodiscard]] T const* operator->() const
	{
		vsm_assert(m_state <= io_result_state::completed_partial);
		return &static_cast<T const&>(m_value);
	}
};

template<>
class io_result2<void>
{
	using E = std::error_code;

protected:
	io_result_error m_error;
	uint8_t m_state;

public:
	using value_type = void;
	using error_type = E;


	io_result2(io_pending_t)
		: m_error{ .dummy = {} }
		, m_state(io_result_state::pending)
	{
	}

	io_result2()
		: m_error{ .dummy = {} }
		, m_state(io_result_state::completed)
	{
	}

	io_result2(io_partial_t)
		: m_error{ .dummy = {} }
		, m_state(io_result_state::completed_partial)
	{
	}

	io_result2(vsm::result<void> const& result)
		: m_error{ .dummy = {} }
	{
		if (result)
		{
			m_state = io_result_state::completed;
		}
		else
		{
			m_state = io_result_state::failure;
			m_error.error = result.error();
		}
	}

	template<typename U>
		requires std::is_constructible_v<E, U const&>
	explicit(!std::is_convertible_v<U const&, E>)
		io_result2(vsm::unexpected<U> const& e)
		: m_error{ .error = e.error() }
		, m_state(io_result_state::failure)
	{
	}

	template<typename U>
		requires std::is_constructible_v<E, U const&>
	explicit(!std::is_convertible_v<U const&, E>)
		io_result2(multiplexer_busy_t, vsm::unexpected<U> const& e)
		: m_error{ .error = e.error() }
		, m_state(io_result_state::multiplexer_busy)
	{
	}


	[[nodiscard]] bool has_value() const
	{
		return m_state <= io_result_state::completed_partial;
	}
	
	[[nodiscard]] bool has_error() const
	{
		return m_state >= io_result_state::pending;
	}

	[[nodiscard]] bool is_pending() const
	{
		return m_state & io_result_state::pending_flag;
	}

	[[nodiscard]] bool is_cancelled() const
	{
		return m_state == io_result_state::cancelled;
	}

	[[nodiscard]] bool is_busy_error() const
	{
		return m_state == io_result_state::multiplexer_busy;
	}

	[[nodiscard]] E const& error() const
	{
		vsm_assert(m_state >= io_result_state::failure);
		return m_error.error;
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_state <= io_result_state::completed_partial;
	}


	void value() const
	{
		vsm_assert(m_state <= io_result_state::completed_partial);
	}

	void operator*() const
	{
		vsm_assert(m_state <= io_result_state::completed_partial);
	}
};

} // namespace allio::detail
