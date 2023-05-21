#pragma once

#include <cassert>

#define vsm_assert(...) \
	assert(__VA_ARGS__)

#define vsm_verify(...) \
	((__VA_ARGS__) ? (void)0 : vsm_assert(false))

#define allio_CHECK(...) \
	vsm_verify(__VA_ARGS__)
