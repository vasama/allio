cmake_minimum_required(VERSION 3.23)

project(allio-openssl)

find_package(OpenSSL QUIET)

if(OpenSSL_FOUND)
	find_package(vsm-intrusive REQUIRED)
	find_package(vsm-numeric REQUIRED)

	vsm_find_sibling(allio-core)

	vsm_add_library(
		allio::openssl

		HEADERS
			include/allio/openssl/socket.hpp

			include/allio/openssl/detail/datagram_socket.hpp
			include/allio/openssl/detail/listen_socket.hpp
			include/allio/openssl/detail/openssl.hpp
			include/allio/openssl/detail/socket.hpp

		SOURCES
			source/allio/impl/openssl/openssl.cpp

		HEADER_DEPENDENCIES
			vsm::intrusive
			allio::core

		SOURCE_DEPENDENCIES
			vsm::numeric
			OpenSSL::SSL

		TEST_SOURCES
			source/allio/test/openssl.cpp
			source/allio/test/socket.cpp

		TEST_DEPENDENCIES
			allio::defaults
	)
	target_include_directories(allio_openssl
		PRIVATE ../../core/source
	)
	target_include_directories(allio_openssl_test
		PRIVATE ../../core/source
	)
endif()
