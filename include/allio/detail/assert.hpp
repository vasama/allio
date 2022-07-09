#pragma once

#include <cassert>

#define allio_ASSERT(...) \
	assert(__VA_ARGS__)

#define allio_VERIFY(...) \
	((__VA_ARGS__) ? (void)0 : allio_ASSERT(false))
