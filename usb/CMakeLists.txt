cmake_minimum_required(VERSION 3.23.0)

project(allio_usb)

add_library(allio_usb)

target_sources(allio_usb
	PUBLIC
	FILE_SET HEADERS
	BASE_DIRS include
	FILES
		include/allio/usb_handle.hpp

	PRIVATE
		source/allio/impl/usb_handle.cpp
)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	target_sources(allio_usb
		PRIVATE
			source/win32/iocp_usb_cdc_handle.cpp
	)
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	target_sources(allio_usb
		PRIVATE
			source/linux/io_uring_usb_cdc_handle.cpp
	)
endif()

target_include_directories(allio_usb
	PUBLIC
		include
)
