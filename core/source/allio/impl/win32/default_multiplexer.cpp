#include <allio/default_multiplexer.hpp>

#include <allio/win32/iocp_multiplexer.hpp>

#include <vsm/utility.hpp>

using namespace allio;
using namespace allio::win32;

vsm::result<unique_multiplexer_ptr> allio::create_default_multiplexer(default_multiplexer_options const& options)
{
	vsm_try(result, iocp_multiplexer::init(
	{
	}));
	return std::make_unique<iocp_multiplexer>(vsm_move(result));
}
