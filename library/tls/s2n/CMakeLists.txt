cmake_minimum_required(VERSION 3.23)
project(allio-s2n)

find_package(vsm.cmake REQUIRED)
vsm_define_package(allio.s2n)

vsm_add_library(
	allio::s2n

	HEADERS
		include/allio/s2n/socket.hpp

		include/allio/s2n/detail/datagram_socket.hpp
		include/allio/s2n/detail/listen_socket.hpp
		include/allio/s2n/detail/s2n.hpp
		include/allio/s2n/detail/socket.hpp

	SOURCES
		source/allio/impl/s2n/s2n.cpp
		source/allio/impl/s2n/s2n.hpp

	HEADER_LINK_LIBRARIES
		vsm::intrusive
		allio::core

	SOURCE_LINK_LIBRARIES
		s2n::s2n

	TEST_SOURCES
		source/allio/test/s2n.cpp
		source/allio/test/socket.cpp

	TEST_LINK_LIBRARIES
		allio::blocking
		allio::senders
)

# TODO: Handle allio.s2n include paths a little smarter:
target_include_directories(allio_s2n PRIVATE ../core/source)
target_include_directories(allio_s2n_test_suite PRIVATE ../core/source)
target_include_directories(allio_s2n_test_suite PRIVATE ../allio/source)
