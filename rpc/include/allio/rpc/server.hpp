#pragma once

#include <vsm/intrusive/rb_tree.hpp>

namespace allio::rpc {

template<typename Channel>
class basic_server
{
	class handler_type
	{
		std::string_view m_identifier;
		
	};

	struct key_selector
	{
		std::string_view vsm_static_operator_invoke(handler_type const& handler)
		{
			return handler.m_identifier;
		}
	};

	Channel m_channel;
	vsm::intrusive::rb_tree<handler_type, key_selector> m_handlers;

public:
	auto receive() &
	{
		
	}
};

} // namespace allio::rpc
