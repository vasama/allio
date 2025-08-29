#pragma once

#include <allio/error.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/timeout.hpp>

#include <vsm/result.hpp>

#include <signal.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

inline vsm::result<void> thread_sigmask(
	int const how,
	sigset_t const* const new_sigset,
	sigset_t* const old_sigset)
{
	if (int const e = pthread_sigmask(how, new_sigset, old_sigset))
	{
		return vsm::unexpected(allio_error(static_cast<system_error>(e)));
	}

	return {};
}

class unique_thread_sigmask
{
	sigset_t m_old_sigset;
	bool m_has_old_sigset = false;

public:
	unique_thread_sigmask() = default;

	unique_thread_sigmask(unique_thread_sigmask const&) = delete;
	unique_thread_sigmask& operator=(unique_thread_sigmask const&) = delete;

	~unique_thread_sigmask()
	{
		if (m_has_old_sigset)
		{
			detail::unrecoverable(_reset());
		}
	}

	[[nodiscard]] bool is_active() const
	{
		return m_has_old_sigset;
	}

	[[nodiscard]] vsm::result<void> set(int const mode, sigset_t const* const sigset)
	{
		vsm_assert(!m_has_old_sigset);

		vsm_try_void(linux::thread_sigmask(
			mode,
			sigset,
			&m_old_sigset));

		m_has_old_sigset = true;
		return {};
	}

	[[nodiscard]] vsm::result<void> set(sigset_t const* const sigset)
	{
		return set(SIG_SETMASK, sigset);
	}

	[[nodiscard]] vsm::result<void> block(sigset_t const* const sigset)
	{
		return set(SIG_BLOCK, sigset);
	}

	[[nodiscard]] vsm::result<void> unblock(sigset_t const* const sigset)
	{
		return set(SIG_UNBLOCK, sigset);
	}

	vsm::result<void> reset()
	{
		if (m_has_old_sigset)
		{
			vsm_try_void(_reset());
		}

		m_has_old_sigset = false;
		return {};
	}

private:
	vsm::result<void> _reset()
	{
		return linux::thread_sigmask(
			SIG_SETMASK,
			&m_old_sigset,
			/* old_sigset: */ nullptr);
	}
};

class sigpipe_handler
{
	sigset_t m_old_sigset;
	bool m_has_old_sigset = false;
	bool m_expects_signal = false;

public:
	sigpipe_handler() = default;

	sigpipe_handler(sigpipe_handler const&) = delete;
	sigpipe_handler& operator=(sigpipe_handler const&) = delete;

	~sigpipe_handler()
	{
		if (m_has_old_sigset)
		{
			if (m_expects_signal)
			{
				detail::unrecoverable(_handle());
			}

			detail::unrecoverable(_deactivate());
		}
	}

	[[nodiscard]] vsm::result<void> activate()
	{
		if (!m_has_old_sigset)
		{
			vsm_try_void(linux::thread_sigmask(
				SIG_BLOCK,
				_get_sigset(),
				&m_old_sigset));

			m_has_old_sigset = true;
		}

		return {};
	}

	void expect_signal()
	{
		m_expects_signal = true;
	}

	[[nodiscard]] vsm::result<void> deactivate()
	{
		if (m_has_old_sigset)
		{
			vsm_try_void(_deactivate());
			m_has_old_sigset = true;
		}

		return {};
	}

	[[nodiscard]] vsm::result<void> handle()
	{
		if (m_expects_signal)
		{
			vsm_try_void(_handle());
			m_expects_signal = false;
		}

		return {};
	}

private:
	[[nodiscard]] vsm::result<void> _handle()
	{
		int const signum = ::sigtimedwait(
			_get_sigset(),
			/* info: */ nullptr,
			kernel_timeout<timespec>::instant());

		if (signum < 0)
		{
			if (auto const e = errno; e != EAGAIN)
			{
				return vsm::unexpected(allio_error(static_cast<system_error>(e)));
			}
		}
		else
		{
			vsm_assert(signum == SIGPIPE);
		}

		return {};
	}

	[[nodiscard]] vsm::result<void> _deactivate()
	{
		return linux::thread_sigmask(
			SIG_SETMASK,
			&m_old_sigset,
			/* old_sigset: */ nullptr);
	}

	[[nodiscard]] sigset_t const* _get_sigset();
};

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
