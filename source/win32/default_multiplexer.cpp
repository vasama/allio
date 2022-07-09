#include <allio/default_multiplexer.hpp>

#include <allio/win32/iocp_multiplexer.hpp>

using namespace allio;
using namespace allio::win32;

result<unique_multiplexer> allio::create_default_multiplexer(default_multiplexer_options const& options)
{
	allio_TRY(result, iocp_multiplexer::init(
	{
	}));
	return std::make_unique<iocp_multiplexer>(std::move(result));
}
