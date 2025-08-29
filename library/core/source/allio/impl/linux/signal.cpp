#include <allio/impl/linux/signal.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

namespace {

struct initialized_sigset : sigset_t
{
	initialized_sigset(int const signum)
	{
		sigemptyset(this);
		sigaddset(this, signum);
	}
};

} // namespace

sigset_t const* sigpipe_handler::_get_sigset()
{
	static initialized_sigset sigset(SIGPIPE);
	return &sigset;
}
