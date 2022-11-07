#pragma once

#define allio_detail_EMPTY(...)

#define allio_detail_EXPAND(...) __VA_ARGS__

#define allio_detail_CAT_1(a, b) a ## b
#define allio_detail_CAT(a, b) allio_detail_CAT_1(a, b)

#define allio_detail_STR_1(...) #__VA_ARGS__
#define allio_detail_STR(...) allio_detail_STR_1(__VA_ARGS__)

#define allio_detail_COUNTER __COUNTER__
