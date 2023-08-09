#include <allio/test/signal.hpp>

#include <vsm/defer.hpp>

#include <signal.h>

using namespace allio;

static thread_local  tls_last_signum;

static void handler(int const signum, siginfo_t*, void*)
{
	std::atomic_signal_fence(std::memory_order_acquire);

	tls_last_signum = signum;

	std::atomic_signal_fence(std::memory_order_release);
}

bool allio::capture_signal(signal const signal, vsm::function_view<void()> const callback)
{
	vsm_assert(signal == signal::access_violation);
	int const signum = SIGSEGV;

	struct sigaction const new_action =
	{
		.sa_sigaction = handler,
		.sa_flags = SA_SIGINFO,
	};

	tls_last_signum = -1;
	std::atomic_signal_fence(std::memory_order_release);

	// Scope of the signal handler override.
	{
		struct sigaction old_action;
		sigaction(signum, &new_action, &old_action);

		vsm_defer
		{
			sigaction(signum, &old_action, nullptr);
		};

		callback();
	}

	std::atomic_signal_fence(std::memory_order_acquire);
	int const last_signum = tls_last_signum;

	return last_signum == signum;
}