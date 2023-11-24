cmake_minimum_required(VERSION 3.23)

project(allio-openssl)

find_package(OpenSSL QUIET)

if(OpenSSL_FOUND)
	vsm_find_sibling(allio-core)

	vsm_add_library(allio::openssl
		HEADERS
			include/allio/openssl/socket.hpp

			include/allio/openssl/detail/datagram_socket.hpp
			include/allio/openssl/detail/listen_socket.hpp
			include/allio/openssl/detail/openssl.hpp
			include/allio/openssl/detail/socket.hpp

		SOURCES
			source/allio/impl/openssl/openssl.cpp

		HEADER_DEPENDENCIES
			allio::core

		SOURCE_DEPENDENCIES
			OpenSSL::SSL

		TEST_SOURCES
			source/allio/test/openssl.cpp
			source/allio/test/socket.cpp
	)
	target_include_directories(allio_openssl
		PRIVATE ../../allio/source
	)
	target_include_directories(allio_openssl_test
		PRIVATE ../../allio/source
	)
endif()
