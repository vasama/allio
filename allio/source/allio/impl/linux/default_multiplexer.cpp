#include <allio/default_multiplexer.hpp>

#include <allio/linux/io_uring_multiplexer.hpp>

#include <vsm/utility.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

vsm::result<unique_multiplexer_ptr> allio::create_default_multiplexer(default_multiplexer_options const& options)
{
	if (has_io_uring())
	{
		vsm_try(result, io_uring_multiplexer::init(
		{
		}));
		return std::make_unique<io_uring_multiplexer>(vsm_move(result));
	}

	return vsm::unexpected(error::unsupported_operation);
}
