#pragma once

#include <unistd.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

struct fork_exec_data
{
	int exec_flags = 0;
	int exec_base = -1;
	int wdir_base = -1;

	int exec_stdin = -1;
	int exec_stdout = -1;
	int exec_stderr = -1;

	bool fork_detached = false;
	bool duplicate_fd = false;
	bool inheritable_fd = false;

	char const* exec_path = nullptr;
	char const* wdir_path = nullptr;

	char* const* exec_argv = nullptr;
	char* const* exec_envp = nullptr;

	int pid_fd = -1;
	int dup_fd = -1;

	pid_t pid = 0;
};

int fork_exec(fork_exec_data& data);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
