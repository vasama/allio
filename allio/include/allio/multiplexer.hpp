#pragma once

#include <allio/detail/parameters.hpp>

#include <vsm/flags.hpp>

#include <cstdint>

namespace allio {

template<typename M, typename H, typename O = void>
concept multiplexer_for = requires
{
};


struct attach_t
{
	template<typename M, typename H, typename S>
	vsm::result<void> vsm_static_operator_invoke(M& multiplexer, H& handle)
		requires tag_invocable<attach_t, M&, H&>
	{
		return tag_invoke(multiplexer, handle);
	}
};

struct detach_t
{
	template<typename M, typename H, typename S>
	vsm::result<void> vsm_static_operator_invoke(M& multiplexer, H& handle)
		requires tag_invocable<detach_t, M&, H&>
	{
		return tag_invoke(multiplexer, handle);
	}
};


struct submit_t
{
	template<typename M, typename H, typename S>
	vsm::result<void> vsm_static_operator_invoke(M& multiplexer, H& handle, S& state)
		requires tag_invocable<submit_t, M&, H&, S&>
	{
		return tag_invoke(multiplexer, handle, state);
	}
};

struct cancel_t
{
	template<typename M, typename H, typename S>
	vsm::result<void> vsm_static_operator_invoke(M& multiplexer, H& handle, S& state)
		requires tag_invocable<cancel_t, M&, H&, S&>
	{
		return tag_invoke(multiplexer, handle, state);
	}
};


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
