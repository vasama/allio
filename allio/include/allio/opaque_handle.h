#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct allio_opaque_handle
{
	uintptr_t handle;
	uintptr_t information;
};

#ifdef __cplusplus
} // extern "C"
#endif
