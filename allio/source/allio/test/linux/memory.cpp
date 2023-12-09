#include <vsm/assert.h>

#include <atomic>
#include <csetjmp>

#include <signal.h>

static thread_local jmp_buf* tls_jmp_buf;
static void handler(int const signum, siginfo_t*, void*)
{
	if (signum == SIGSEGV)
	{
		std::atomic_signal_fence(std::memory_order_acquire);
		jmp_buf* const buf = tls_jmp_buf;

		if (buf != nullptr)
		{
			tls_jmp_buf = nullptr;
			std::atomic_signal_fence(std::memory_order_release);

			longjmp(*buf, 1);
		}
	}
}

namespace allio::test {

bool catch_access_violation(void(*function)(void*), void* context);

bool catch_access_violation(void(* const function)(void*), void* const context)
{
	bool r = true;

	struct sigaction new_action = {};
	new_action.sa_sigaction = handler;
	new_action.sa_flags = SA_SIGINFO;

	struct sigaction old_action;
	vsm_assert(sigaction(SIGSEGV, &new_action, &old_action) != -1);

	if (jmp_buf buf; setjmp(buf) == 0)
	{
		tls_jmp_buf = &buf;
		std::atomic_signal_fence(std::memory_order_release);

		function(context);

		tls_jmp_buf = nullptr;
		std::atomic_signal_fence(std::memory_order_release);
	}
	else
	{
		r = false;
	}

	vsm_assert(sigaction(SIGSEGV, &old_action, nullptr) != -1);

	return r;
}

} // namespace allio::test
