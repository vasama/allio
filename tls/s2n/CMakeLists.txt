cmake_minimum_required(VERSION 3.23)

project(allio-s2n)

find_package(s2n QUIET)

if(s2n_FOUND)
	vsm_find_sibling(allio-core)

	vsm_add_library(allio::s2n
		HEADERS
			include/allio/s2n/detail/s2n.hpp

		SOURCES
			source/allio/impl/s2n/s2n.cpp

		HEADER_DEPENDENCIES
			allio::core

		SOURCE_DEPENDENCIES
			s2n::s2n

		TEST_SOURCES
			source/allio/test/s2n.cpp
	)
	target_include_directories(allio_s2n
		PRIVATE ../../allio/source
	)
	target_include_directories(allio_s2n_test
		PRIVATE ../../allio/source
	)
endif()
