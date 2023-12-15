#pragma once

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/linux/detail/io_uring/io_uring.hpp>

#include <vsm/lazy.hpp>
#include <vsm/result.hpp>

#include <sys/syscall.h>
#include <unistd.h>

namespace allio::linux {

using namespace detail::io_uring_constants;


inline int _io_uring_setup(unsigned const entries, io_uring_params* const args)
{
	return syscall(__NR_io_uring_setup, entries, args);
}

inline int _io_uring_enter(int const fd, unsigned const to_submit, unsigned const min_complete, unsigned const flags, void const* const arg, size_t const argsz)
{
	return syscall(__NR_io_uring_enter, fd, to_submit, min_complete, flags, arg, argsz);
}

inline int _io_uring_register(int const fd, unsigned const opcode, void* const arg, unsigned const nr_args)
{
	return syscall(__NR_io_uring_register, fd, opcode, arg, nr_args);
}


inline vsm::result<detail::unique_handle> io_uring_setup(
	unsigned const entries,
	io_uring_params& setup)
{
	int const fd = _io_uring_setup(entries, &setup);

	if (fd == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm_lazy(detail::unique_handle(fd));
}

template<typename Argument = io_uring_getevents_arg>
inline vsm::result<uint32_t> io_uring_enter(
	int const fd,
	unsigned const to_submit,
	unsigned const min_complete,
	unsigned const flags,
	std::type_identity_t<io_uring_getevents_arg> const* arg = nullptr)
{
	int const consumed = _io_uring_enter(
		fd,
		to_submit,
		min_complete,
		flags,
		arg,
		arg != nullptr ? sizeof(Argument) : 0);

	if (consumed == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return static_cast<uint32_t>(consumed);
}

} // namespace allio::linux
