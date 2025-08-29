#pragma once

#include <cstddef>

#include <unistd.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

struct fork_exec_data
{
	// Flags passed directly to execveat.
	int exec_flags = 0;

	// Base descriptor passed directly to execveat.
	int exec_base = -1;

	// Base descriptor for the child process working directory path.
	int wdir_base = -1;

	// If any of these descriptors is not -1, it is duplicated into the corresponding standard
	// stream descriptor position (STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO) during the fork-exec
	// procedure. This causes any existing descriptor in that position to be closed.
	int exec_stdin = -1;
	int exec_stdout = -1;
	int exec_stderr = -1;

	// If this flag is set, the new process is not launched as a direct child of the calling
	// process. Instead its parent is the init process. This means that it does not have to be
	// reaped, but also that its eventual exit code becomes unavailable.
	bool fork_detached = false;

	// If this flag is set, the primary pid fd (pid_fd) for the new process does not have its
	// CLOEXEC flag set. Note that if a duplicate fd is requested (via duplicate_fd) the secondary
	// pid fd (dup_fd) will have its CLOEXEC flag set regardless.
	bool inheritable_fd = false;

	// If this flag is set, a second duplicate pid fd is created and returned via dup_fd.
	bool duplicate_fd = false;

	// Executable path passed directly to execveat. If the path is relative, and exec_base is not
	// -1, it is relative to the directory referred to by exec_base. Otherwise, the path is relative
	// to the current working directory.
	char const* exec_path = nullptr;

	// Path to the child process working directory. If the path is relative, and wdir_base is not
	// -1, it is relative to the directory referred to by wdir_base. Otherwise, the path is relative
	// to the current working directory.
	char const* wdir_path = nullptr;

	// List of command-line arguments passed directly to execveat.
	char* const* exec_argv = nullptr;

	// List of environment variables passed directly to execveat.
	char* const* exec_envp = nullptr;

	// Array of fds to be inherited by the child process. Any open fd not found in this array is
	// closed during the fork-exec procedure. The array must be sorted in ascending order. If the
	// array pointer is null, no descriptors are closed. Otherwise, if the array size is zero, all
	// open descriptors are closed.
	int const* inherit_fd_array = nullptr;

	// Size (in file descriptors) of the inherited fd array.
	size_t inherit_fd_count = 0;


	// The following values are initialized if fork_exec succeeds:

	// This fd is always initialized. It refers to the child process. If inheritability was
	// requested (via inheritable_fd), this fd does not have its CLOEXEC flag set.
	int pid_fd = -1;

	// This fd is initialized only if a duplicate fd was requested. It refers to the child process.
	// Its CLOEXEC flag is always set, regardless of whether inheritability was requested.
	int dup_fd = -1;

	// This pid is always initialized. It refers to the child process.
	pid_t pid = 0;
};

int fork_exec(fork_exec_data& data);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
