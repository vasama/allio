cmake_minimum_required(VERSION 3.24)
project(allio_allio)

find_package(vsm.cmake REQUIRED)
vsm_define_package(allio)

vsm_add_library(
	allio::allio

	HEADER_DEPENDENCIES
		allio::core
		allio::blocking
		allio::senders

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
	allio::allio
	PLATFORM Windows

	TEST_SOURCES
		source/allio/test/win32/memory.cpp
)

vsm_configure(
	allio::allio
	PLATFORM Linux

	TEST_SOURCES
		source/allio/test/linux/memory.cpp
)

get_property(
	test_target
	TARGET allio::allio-test-suite
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
