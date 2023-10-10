#pragma once

#define allio_detail_default_lifetime(T) \
	T() = default; \
	T(T&&) = default; \
	T& operator=(T&&) & = default; \
	~T() = default \
