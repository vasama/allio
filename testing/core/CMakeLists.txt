cmake_minimum_required(VERSION 3.24)
project(allio_testing_core)

find_package(vsm.cmake REQUIRED)
vsm_define_package(allio.testing.core)

vsm_add_library(
	allio::testing::core

	HEADERS
		include/allio/testing/exception.hpp
		include/allio/testing/filesystem.hpp
		include/allio/testing/match_error.hpp
		include/allio/testing/memory.hpp
		include/allio/testing/network.hpp
		include/allio/testing/shared_object.hpp
		include/allio/testing/spawn.hpp

	SOURCES
		source/allio/impl/filesystem.cpp
		source/allio/impl/network.cpp

	HEADER_LINK_LIBRARIES
		allio::core
		Catch2::Catch2
)

vsm_configure(
	allio::testing::core
	PLATFORM Windows

	SOURCES
		source/allio/impl/win32/filesystem.cpp
		source/allio/impl/win32/memory.cpp
)

vsm_configure(
	allio::testing::core
	PLATFORM Linux

	SOURCES
		source/allio/impl/linux/filesystem.cpp
		source/allio/impl/linux/memory.cpp
)
