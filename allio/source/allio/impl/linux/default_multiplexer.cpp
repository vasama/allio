#include <allio/default_multiplexer.hpp>

#include <allio/linux/io_uring_multiplexer.hpp>

#include <vsm/utility.hpp>

#include <sys/syscall.h>
#include <unistd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

static bool is_io_uring_supported()
{
	static bool is_supported = []() -> bool
	{
		errno = 0;
		syscall(__NR_io_uring_register, 0, IORING_UNREGISTER_BUFFERS, NULL, 0);
		return errno != ENOSYS;
	}();
	return is_supported;
}

vsm::result<unique_multiplexer_ptr> allio::create_default_multiplexer(default_multiplexer_options const& options)
{
	if (is_io_uring_supported())
	{
		vsm_try(result, io_uring_multiplexer::init(
		{
			.enable_concurrent_submission = options.enable_concurrent_submission,
			.enable_concurrent_completion = options.enable_concurrent_completion,
		}));
		return std::make_unique<io_uring_multiplexer>(vsm_move(result));
	}

	return std::unexpected(error::unsupported_operation);
}
