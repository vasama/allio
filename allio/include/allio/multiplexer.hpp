#pragma once

#include <allio/detail/parameters.hpp>

#include <vsm/assert.h>
#include <vsm/atomic.hpp>
#include <vsm/flags.hpp>

#include <cstdint>

namespace allio {

enum class poll_mode : uint8_t
{
	none                            = 0,

	submit                          = 1 << 0,
	complete                        = 1 << 1,
	flush                           = 1 << 2,

	all                             = submit | complete | flush,
};
vsm_flag_enum(poll_mode);


struct poll_statistics
{
	size_t submitted;
	size_t completed;
	size_t concluded;
};

class local_poll_statistics
{
	poll_statistics m_statistics;
	poll_statistics* m_p_statistics;

public:
	explicit local_poll_statistics(poll_statistics* const p_statistics)
	{
		if (p_statistics != nullptr)
		{
			m_p_statistics = p_statistics;
		}
		else
		{
			m_statistics = {};
			m_p_statistics = &m_statistics;
		}
	}

	local_poll_statistics(local_poll_statistics const&) = delete;
	local_poll_statistics& operator=(local_poll_statistics const&) = delete;

	operator poll_statistics&()
	{
		return *m_p_statistics;
	}
};


#define allio_multiplexer_poll_parameters(type, data, ...) \
	type(::allio, poll_parameters) \
	data(::allio::poll_mode,            mode,               poll_mode::all) \
	data(::allio::poll_statistics*,     statistics,         nullptr) \
	allio_deadline_parameters(__VA_ARGS__, __VA_ARGS__) \

allio_interface_parameters(allio_multiplexer_poll_parameters);

} // namespace allio
