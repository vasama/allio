cmake_minimum_required(VERSION 3.24)
project(allio_blocking)

find_package(vsm.cmake REQUIRED)
vsm_define_package(allio.blocking)

vsm_add_library(
	allio::blocking

	HEADERS
		include/allio/default_multiplexer.hpp

		include/allio/blocking/mapped.hpp

		# Handles
		include/allio/blocking/directory.hpp
		include/allio/blocking/event.hpp
		include/allio/blocking/file.hpp
		include/allio/blocking/map.hpp
		include/allio/blocking/pipe.hpp
		include/allio/blocking/process.hpp
		include/allio/blocking/raw_datagram_socket.hpp
		include/allio/blocking/raw_listen_socket.hpp
		include/allio/blocking/raw_socket.hpp
		include/allio/blocking/section.hpp
		include/allio/blocking/traits.hpp

	SOURCES
		source/allio/impl/exceptions.cpp

		# Handles
		source/allio/impl/handles/standard_stream.cpp

	HEADER_DEPENDENCIES
		allio::core
)
