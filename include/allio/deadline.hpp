#pragma once

#include <chrono>

#include <cstdint>

namespace allio {

class deadline
{
	// High bit 0: relative
	// High bit 1: absolute
	// Low bits: nanoseconds from some epoch or operation start.
	uint64_t m_bits;

public:
	deadline() = default;

	static constexpr deadline instant()
	{
		return deadline(0);
	}

	static constexpr deadline never()
	{
		return deadline(static_cast<uint64_t>(-1));
	}

	bool operator==(deadline const&) const = default;

private:
	explicit constexpr deadline(uint64_t const bits)
		: m_bits(bits)
	{
	}
};

} // namespace allio
