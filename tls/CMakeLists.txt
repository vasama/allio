cmake_minimum_required(VERSION 3.23.0)

project(allio_tls)

add_library(allio_tls)

target_sources(allio_tls
	PUBLIC
	FILE_SET HEADERS
	BASE_DIRS include
	FILES
		include/allio/tls_socket_handle.hpp

	PRIVATE
		source/allio/impl/openssl.cpp
		source/allio/impl/tls_socket_handle.cpp
)

target_include_directories(allio_tls
	PUBLIC
		include
)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	target_sources(allio_tls
		PRIVATE
			source/allio/impl/win32/schannel.cpp
	)
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	target_sources(allio_tls
		PRIVATE
			source/allio/impl/s2n.cpp
	)
endif()
