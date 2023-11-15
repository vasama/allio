cmake_minimum_required(VERSION 3.23)

project(allio-openssl)

find_package(OpenSSL QUIET)

if(OpenSSL_FOUND)
	vsm_find_sibling(allio)

	vsm_add_library(allio-openssl
		HEADERS
			include/allio/openssl/socket.hpp

			include/allio/detail/openssl/datagram_socket.hpp
			include/allio/detail/openssl/listen_socket.hpp
			include/allio/detail/openssl/openssl.hpp
			include/allio/detail/openssl/socket.hpp

		SOURCES
			source/allio/impl/openssl/socket.cpp

		HEADER_DEPENDENCIES
			allio

		SOURCE_DEPENDENCIES
			OpenSSL::SSL

		TEST_SOURCES
			source/allio/test/socket.cpp
	)
endif()
