#pragma once

#include <vsm/atomic.hpp>

#include <concepts>

namespace allio::detail {

class lazy_feature_flag
{
	static constexpr uint8_t lazy_init_flag = 0x80;
	static constexpr uint8_t supported_flag = 0x01;

	uint8_t m_value = 0;

public:
	consteval lazy_feature_flag() = default;

	template<typename Query>
	[[nodiscard]] bool query(Query&& query)
	{
		auto const reference = vsm::atomic_ref(m_value);
		uint8_t value = reference.load(std::memory_order_acquire);

		if (value == 0)
		{
			bool const supported = vsm_forward(query)();
			value = lazy_init_flag | (supported ? supported_flag : 0);
			reference.store(value, std::memory_order_release);
		}

		return value & supported_flag;
	}
};

} // namespace allio::detail
