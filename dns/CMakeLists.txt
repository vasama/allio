cmake_minimum_required(VERSION 3.23.0)

project(allio_dns)

add_library(allio_dns)

target_sources(allio_dns
	PUBLIC
	FILE_SET HEADERS
	BASE_DIRS include
	FILES
		include/allio/dns_cache.hpp
		include/allio/dns_resolver.hpp

	PRIVATE
		source/allio/impl/dns_resolver.cpp
)

target_include_directories(allio_dns
	PUBLIC
		include
)
