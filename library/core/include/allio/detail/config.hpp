#pragma once

#define allio_ntapi_never               0
#define allio_ntapi_required            1
#define allio_ntapi_always              2

#ifndef allio_config_ntapi
#	define allio_config_ntapi           allio_ntapi_required
#endif
