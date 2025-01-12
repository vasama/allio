#include <allio/impl/linux/process_reaper.hpp>

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/linux/epoll.hpp>
#include <allio/impl/linux/eventfd.hpp>
#include <allio/impl/new.hpp>

#include <vsm/atomic.hpp>
#include <vsm/intrusive/mpsc_queue.hpp>

#include <optional>
#include <span>
#include <thread>

#include <sys/wait.h>
#include <linux/wait.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

namespace {

enum class exit_state : uint8_t
{
	exit_pending,
	exit_code_available,
	exit_code_not_available,
};

} // namespace

struct detail::unix_process_reaper : vsm::intrusive::mpsc_queue_link
{
	vsm::atomic<size_t> refcount = 1;
	vsm::atomic<::exit_state> exit_state = ::exit_state::exit_pending;

	int fd = -1;
	int exit_code = 0;
};

vsm::result<int, system_error> process_wait(int const fd, int const flags)
{
	siginfo_t siginfo;
	int const r = waitid(
		static_cast<idtype_t>(P_PIDFD),
		static_cast<id_t>(fd),
		&siginfo,
		flags | WEXITED | WSTOPPED);

	if (r == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	if (r == 0)
	{
		vsm_assert(flags & WNOHANG);
		return vsm::unexpected(error::operation_timed_out);
	}

	//TODO: Should the exit code be different when the process was killed?
	//      See what other libraries are doing here.

	// If si_status describes a signal, add 128 to match common shell behaviour. This is used to
	// distinguish between user-specified exit codes in the range [0, 127] and signals. Otherwise
	// the user-specified exit code is truncated to its low 8 bits. This is most likely redundant,
	// as the kernel has already truncated it, but it is also done here as a defensive measure to
	// proof against an allio API break due to future kernels returning the full exit code.
	return siginfo.si_code == CLD_EXITED
		? siginfo.si_status & 0xFF
		: siginfo.si_status + 128;
}

static std::optional<int> process_wait_or_echild(int const fd, int const flags)
{
	auto const r = process_wait(fd, flags);

	if (!r)
	{
		// Perhaps a rogue user already waited the process. In any case it is now waited and can be
		// released.
		vsm_assert(errno == ECHILD);

		return std::nullopt;
	}

	return *r;
}

namespace {

struct reaper_thread
{
	vsm::intrusive::mpsc_queue<process_reaper> m_shared_queue;
	vsm::intrusive::forward_list<process_reaper> m_local_queue;

	unique_handle m_event;
	unique_handle m_epoll;

	vsm::atomic<bool> m_exit_requested = false;
	std::thread m_thread;

public:
	reaper_thread() = default;

	reaper_thread(reaper_thread const&) = delete;
	reaper_thread& operator=(reaper_thread const&) = delete;

	~reaper_thread()
	{
		if (m_thread.joinable())
		{
			m_exit_requested.store(true, std::memory_order_release);
			vsm_verify(linux::eventfd_write(m_event.get(), 1));
			m_thread.join();
		}

		splice_shared_queue();
		while (!m_local_queue.empty())
		{
			release_process_reaper(&m_local_queue.pop_front());
		}
	}

	vsm::result<void> initialize()
	{
		vsm_try_assign(m_event, linux::eventfd(EFD_CLOEXEC | EFD_NONBLOCK));
		vsm_try_assign(m_epoll, linux::epoll_create());

		// Arm the epoll wait for m_event.
		{
			epoll_event event =
			{
				.events = EPOLLIN,
			};
			vsm_try_void(linux::epoll_ctl(
				m_epoll.get(),
				EPOLL_CTL_ADD,
				m_event.get(),
				&event));
		}

		try
		{
			//TODO: Reduce the thread stack size to bare minimum.
			m_thread = std::thread([this]() { thread_start(); });
		}
		catch (std::system_error const& e)
		{
			return vsm::unexpected(e.code());
		}

		return {};
	}

	void register_process(process_reaper* const process)
	{
		(void)process->refcount.fetch_add(1, std::memory_order_relaxed);

		if (!create_process_wait(process))
		{
			if (m_shared_queue.push_back(*process))
			{
				vsm_verify(linux::eventfd_write(m_event.get(), 1));
			}
		}
	}

private:
	void thread_start()
	{
		while (true)
		{
			epoll_event events[16];
			int const ready = epoll_wait(
				m_epoll.get(),
				events,
				std::size(events),
				/* timeout: */ -1);
			vsm_assert(ready != -1);

			if (m_exit_requested.load(std::memory_order_acquire))
			{
				break;
			}

			for (auto const& event : std::span(events, static_cast<size_t>(ready)))
			{
				// The event is polled with a null event data.
				if (event.data.ptr == nullptr)
				{
					// Reset the event so the poll may fire again.
					vsm_verify(linux::eventfd_read(m_event.get()));

					// The event is signaled after new processes have been pushed into the shared
					// queue.
					splice_shared_queue();
				}
				else
				{
					// A child process exited. Wait it to release its zombified pid.
					auto const process = static_cast<process_reaper*>(event.data.ptr);

					if (auto const exit_code = process_wait(process->fd, WNOHANG))
					{
						process->exit_code = *exit_code;
						process->exit_state.store(
							exit_state::exit_code_available,
							std::memory_order_release);
					}
					else
					{
						// Any error besides ECHILD is unexpected and unrecoverable. ECHILD means a
						// rogue user must have waited the process manually, in which case the
						// process has terminated but the exit code is not available.
						if (r.error() != static_cast<system_error>(ECHILD))
						{
							unrecoverable_error(r.error());
						}

						process->exit_state.store(
							exit_state::exit_code_not_available,
							std::memory_order_release);
					}

					vsm_verify(close(process->fd) != -1);
					process->fd = -1;

					release_process_reaper(process);
				}
			}

			// Flush the local queue into the epoll.
			while (!m_local_queue.empty())
			{
				if (create_process_wait(&m_local_queue.front()))
				{
					(void)m_local_queue.pop_front();
				}
			}
		}
	}

	bool create_process_wait(process_reaper* const process)
	{
		epoll_event event =
		{
			.events = EPOLLIN | EPOLLONESHOT,
			.data = { static_cast<void*>(process) },
		};

		return linux::epoll_ctl(
			m_epoll.get(),
			EPOLL_CTL_ADD,
			process->fd,
			&event).has_value();
	}

	void splice_shared_queue()
	{
		// Splice new processes from the shared queue into the local queue.
		m_local_queue.splice_back(m_shared_queue.pop_all_reversed());
	}
};

} // namespace

static reaper_thread g_reaper_thread;

static vsm::result<void> initialize_reaper_thread()
{
	static auto const r = g_reaper_thread.initialize();
	return r;
}

vsm::result<process_reaper_ptr> linux::acquire_process_reaper()
{
	vsm_try_void(initialize_reaper_thread());
	vsm_try(process, make_unique<process_reaper>());
	return vsm_lazy(process_reaper_ptr(process.release()));
}

void linux::release_process_reaper(process_reaper* const process)
{
	if (process->refcount.fetch_sub(1, std::memory_order_acq_rel) == 1)
	{
		if (process->fd != -1)
		{
			vsm_verify(close(process->fd) != -1);
		}
		object_deleter()(process);
	}
}

void linux::start_process_reaper(process_reaper* const process, int const fd)
{
	vsm_assert(fd != -1); //PRECONDITION
	vsm_assert(process->fd == -1); //PRECONDITION

	process->fd = fd;
	g_reaper_thread.register_process(process);
}

vsm::result<std::optional<int>> linux::process_reaper_wait(
	process_reaper* const process,
	int const fd,
	deadline const deadline)
{
	// The reaper thread may mutate process->fd at any point. For this reason the duplicate
	// file descriptor provided by the caller must be used instead.

	// If the process exit is still pending, wait for it without reaping. Even if the wait succeeds,
	// the reaper thread will later reap the child process. The caller is only interested in the
	// fact that the process has terminated and in its exit code.
	if (process->exit_state.load(std::memory_order_acquire) == exit_state::exit_pending)
	{
		int flags = WNOWAIT;

		if (!deadline.is_trivial())
		{
			// Polling is required to specify a non-trivial timeout.
			vsm_try_void(linux::poll(fd, POLLIN, deadline));
		}
		else if (deadline == deadline::instant())
		{
			flags |= WNOHANG;
		}

		auto const r = process_wait(fd, flags);

		if (r)
		{
			return *r;
		}

		if (r.error() != static_cast<system_error>(ECHILD))
		{
			return vsm::unexpected(r.error());
		}

		// In the case of ECHILD, the reaper thread must have won the race to reap the child
		// process. The exit code must now be retrieved from the process reaper shared state.
	}

	while (true)
	{
		switch (process->exit_state.load(std::memory_order_acquire))
		{
		case exit_state::exit_pending:
			break;

		case exit_state::exit_code_available:
			return process->exit_code;

		case exit_state::exit_code_not_available:
			return std::nullopt;
		}

		// Wait until the reaper thread produces the exit code.
		std::this_thread::yield();
	}
}
