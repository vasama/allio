#pragma once

// TODO: Just for debugging
#define allio_config_syscall_hook 1

#if allio_config_syscall_hook
#	include <allio/impl/syscall_hook.hpp>
#endif

#ifndef allio_syscall
#	define allio_syscall(syscall) syscall
#endif
