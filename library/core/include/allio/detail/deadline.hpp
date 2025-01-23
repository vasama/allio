#pragma once

#include <allio/detail/parameters.hpp>

#include <vsm/assert.h>

#include <chrono>

#include <cstdint>

namespace allio::detail {

class deadline
{
#if allio_config_deadline_32ms
	using repr_type = uint32_t;
	using unit_type = std::milli;
#else
	using repr_type = uint64_t;
	using unit_type = std::nano;
#endif

public:
	using clock = std::chrono::steady_clock;

private:
	static constexpr repr_type max_units = static_cast<repr_type>(-1) >> 1;
	static constexpr repr_type absolute_flag = ~max_units;

	static constexpr repr_type never_bits = static_cast<repr_type>(-1);

	// High bit 0: relative.
	// High bit 1: absolute.
	// Low bits: units from
	//   * relative: operation start, or
	//   * absolute: unspecified epoch.
	repr_type m_bits;

public:
	using duration = std::chrono::duration<std::make_signed_t<repr_type>, unit_type>;
	using time_point = std::chrono::time_point<clock, duration>;

	static constexpr duration max_duration = duration(max_units);

	constexpr deadline()
		: m_bits(never_bits)
	{
	}

	constexpr deadline(duration const relative)
		// A negative duration would cause the relative value to become greater than max_units, and
		// the long deadline would then be initialized to truncated to never.
		: deadline(0, static_cast<repr_type>(relative.count()))
	{
	}

	template<typename Repr, typename Unit>
	constexpr deadline(std::chrono::duration<Repr, Unit> const relative)
		requires std::convertible_to<std::chrono::duration<Repr, Unit>, duration>
		: deadline(static_cast<duration>(relative))
	{
	}

	constexpr deadline(time_point const absolute)
		: deadline(absolute_flag, units_since_epoch(absolute))
	{
	}

	template<typename Clock, typename Duration>
	constexpr deadline(std::chrono::time_point<Clock, Duration> const absolute)
		requires std::convertible_to<std::chrono::time_point<Clock, Duration>, time_point>
		: deadline(static_cast<duration>(absolute))
	{
	}


	[[nodiscard]] constexpr bool is_relative() const
	{
		//TODO: VS complains about arithmetic overflow here.
		return static_cast<repr_type>(m_bits + 1) <= absolute_flag;
	}

	[[nodiscard]] constexpr duration relative() const
	{
		vsm_assert(is_relative());
		return duration(m_bits);
	}

	[[nodiscard]] constexpr bool is_absolute() const
	{
		return m_bits - 1 < never_bits - 1;
	}

	[[nodiscard]] constexpr time_point absolute() const
	{
		vsm_assert(is_absolute());
		return time_point(duration(m_bits & max_units));
	}

	[[nodiscard]] constexpr bool is_trivial() const
	{
		// Either 0 or never_bits (-1).
		return static_cast<repr_type>(m_bits + 1) <= static_cast<repr_type>(1);
	}


	[[nodiscard]] deadline start(time_point const now) const
	{
		return _start([&]() { return now; });
	}

	[[nodiscard]] deadline start() const
	{
		return _start([]() { return clock::now(); });
	}


	[[nodiscard]] static consteval deadline instant()
	{
		return deadline(0);
	}

	[[nodiscard]] static consteval deadline never()
	{
		return deadline(never_bits);
	}


	[[nodiscard]] bool operator==(deadline const&) const = default;

private:
	explicit constexpr deadline(repr_type const bits)
		: m_bits(bits)
	{
	}

	explicit constexpr deadline(repr_type const flag, repr_type const units)
		: m_bits(units > max_units ? never_bits : flag | units)
	{
	}


	[[nodiscard]] deadline _start(auto get_now) const
	{
		if (m_bits & absolute_flag)
		{
			return *this;
		}

		repr_type const max = max_units - m_bits;
		repr_type const now = units_since_epoch(get_now());

		return deadline(absolute_flag | (now > max ? max : now + m_bits));
	}


	static constexpr repr_type units_since_epoch(clock::time_point const& time_point)
	{
		return static_cast<repr_type>(
			std::chrono::duration_cast<duration>(time_point.time_since_epoch()).count());
	}
};

struct deadline_t
{
	detail::deadline deadline = detail::deadline::never();

	void set_argument(detail::deadline const value)
	{
		deadline = value;
	}
};

} // namespace allio::detail
