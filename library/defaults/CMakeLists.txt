cmake_minimum_required(VERSION 3.24)
project(allio_defaults)

find_package(vsm.cmake REQUIRED)
vsm_define_package(allio)

vsm_add_library(
	allio::defaults

	HEADERS
		include/allio/default_multiplexer.hpp
		include/allio/event_queue.hpp
		include/allio/sync_wait.hpp
		include/allio/via.hpp

		include/allio/detail/default_traits.hpp
		include/allio/detail/event_queue.hpp
		include/allio/detail/execution.hpp
		include/allio/detail/get_multiplexer.hpp
		include/allio/detail/io_sender.hpp
		include/allio/detail/sync_wait.hpp

		# Handles
		include/allio/directory.hpp
		include/allio/event.hpp
		include/allio/file.hpp
		include/allio/fs_object.hpp
		include/allio/map.hpp
		include/allio/platform_object.hpp
		include/allio/process.hpp
		include/allio/section.hpp
		include/allio/socket.hpp

	SOURCES
		source/allio/impl/exceptions.cpp

	HEADER_DEPENDENCIES
		allio::core
		vsm::intrusive
		stdexec::stdexec

	TEST_SOURCES
		source/allio/test/memory.cpp
		source/allio/test/memory.hpp

		source/allio/test/handles/datagram_socket.cpp
		source/allio/test/handles/directory.cpp
		source/allio/test/handles/event.cpp
		source/allio/test/handles/file.cpp
		source/allio/test/handles/map.cpp
		#source/allio/test/handles/opaque_object.cpp
		source/allio/test/handles/process.cpp
		source/allio/test/handles/socket.cpp
)

vsm_configure(
	allio::defaults
	PLATFORM Windows

	TEST_SOURCES
		source/allio/test/win32/memory.cpp
)

vsm_configure(
	allio::defaults
	PLATFORM Linux

	TEST_SOURCES
		source/allio/test/linux/memory.cpp
)


get_property(
	test_target
	TARGET allio::defaults-test-suite
	PROPERTY ALIASED_TARGET)


add_dependencies("${test_target}" test_exe)
target_include_directories("${test_target}" PRIVATE "../core/source")

target_compile_definitions(
	"${test_target}"

	PRIVATE
		"allio_detail_test_exe=\"$<TARGET_FILE:test_exe>\""
		"allio_detail_test_exe_output=\"$<TARGET_PROPERTY:test_exe,allio_output>\""
)

add_subdirectory(tools)
