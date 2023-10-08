cmake_minimum_required(VERSION 3.23.0)

project(allio)

include(FetchContent)

find_package(vsm-box REQUIRED)
find_package(vsm-core REQUIRED)
find_package(vsm-flags REQUIRED)
find_package(vsm-linear REQUIRED)
find_package(vsm-literals REQUIRED)
find_package(vsm-intrusive REQUIRED)
find_package(vsm-intrusive_ptr REQUIRED)
find_package(vsm-optional REQUIRED)
find_package(vsm-result REQUIRED)
find_package(vsm-tag_ptr REQUIRED)
find_package(vsm-unique_resource REQUIRED)
find_package(p2300 REQUIRED)

vsm_add_library2(allio
	HEADERS
		include/allio/detail/async_io.hpp
		include/allio/detail/any_handle.hpp
		include/allio/detail/api.hpp
		include/allio/detail/dynamic_buffer.hpp
		include/allio/detail/platform.h
		include/allio/detail/string_ref.hpp
		include/allio/detail/type_list.hpp

		include/allio/async_result.hpp
		include/allio/byte_io.hpp
		include/allio/deadline.hpp
		include/allio/default_multiplexer.hpp
		include/allio/directory_handle.hpp
		include/allio/event_handle.hpp
		include/allio/filesystem.hpp
		include/allio/filesystem_handle.hpp
		include/allio/file_handle.hpp
		include/allio/handle.hpp
		include/allio/map_handle.hpp
		include/allio/network.hpp
		include/allio/packet_socket_handle.hpp
		include/allio/path.hpp
		include/allio/path_literal.hpp
		include/allio/path_view.hpp
		include/allio/platform_handle.hpp
		include/allio/platform.hpp
		include/allio/process_handle.hpp
		include/allio/section_handle.hpp
		include/allio/socket_handle.hpp
		include/allio/stream_socket_handle.hpp
		include/allio/step_deadline.hpp
		include/allio/sync_wait.hpp

	SOURCES
		source/allio/impl/error.cpp
		source/allio/impl/network.cpp
		source/allio/impl/transcode.hpp
		source/allio/impl/unique_name.cpp
		source/allio/impl/unique_name.hpp

	HEADER_DEPENDENCIES
		vsm::box
		vsm::core
		vsm::flags
		vsm::linear
		vsm::literals
		vsm::intrusive
		vsm::intrusive_ptr
		vsm::optional
		vsm::result
		vsm::tag_ptr
		vsm::unique_resource
		p2300::p2300

	TEST_SOURCES
		source/allio/test/filesystem.hpp
		source/allio/test/multiplexer.hpp
		source/allio/test/network.cpp
		source/allio/test/path_view.cpp
		source/allio/test/reverse_iterator.hpp
		source/allio/test/sync_wait.cpp
		source/allio/test/task.hpp
	
		#source/allio/test/handles/directory_handle.cpp
		source/allio/test/handles/event_handle.cpp
		#source/allio/test/handles/file_handle.cpp
		#source/allio/test/handles/map_handle.cpp
		#source/allio/test/handles/opaque_handle.cpp
		source/allio/test/handles/process_handle.cpp
		source/allio/test/handles/socket_handle.cpp
)

vsm_configure(allio
	PLATFORM Windows
	
	HEADERS
		include/allio/win32/detail/iocp/byte_io.hpp
		include/allio/win32/detail/iocp/event_handle.hpp
		include/allio/win32/detail/iocp/multiplexer.hpp
		include/allio/win32/kernel_error.hpp
		include/allio/win32/platform.hpp
		include/allio/win32/detail/path.hpp
		include/allio/win32/detail/path_view.hpp
		include/allio/win32/detail/unique_handle.hpp
		include/allio/win32/detail/win32_fwd.hpp

	SOURCES
		source/allio/impl/posix/socket.cpp
		source/allio/impl/posix/socket.hpp
		source/allio/impl/win32/completion_port.cpp
		source/allio/impl/win32/completion_port.hpp
		source/allio/impl/win32/command_line.cpp
		source/allio/impl/win32/command_line.hpp
		source/allio/impl/win32/error.cpp
		source/allio/impl/win32/error.hpp
		source/allio/impl/win32/kernel.cpp
		source/allio/impl/win32/kernel.hpp
		source/allio/impl/win32/kernel_error.cpp
		source/allio/impl/win32/kernel_path.cpp
		source/allio/impl/win32/kernel_path.hpp
		source/allio/impl/win32/kernel_path_impl.hpp
		source/allio/impl/win32/kernel_path_old.hpp
		source/allio/impl/win32/memory.cpp
		source/allio/impl/win32/peb.cpp
		source/allio/impl/win32/peb.hpp
		source/allio/impl/win32/platform.cpp
		source/allio/impl/win32/posix_socket.cpp
		source/allio/impl/win32/posix_socket.hpp
		source/allio/impl/win32/transcode.cpp
		source/allio/impl/win32/unique_handle.cpp
		source/allio/impl/win32/wait_packet.cpp
		source/allio/impl/win32/wait_packet.hpp
		source/allio/impl/win32/wsa_ex.cpp
		source/allio/impl/win32/wsa_ex.hpp

		source/allio/impl/posix/handles/common_socket_handle.cpp
		source/allio/impl/posix/handles/listen_socket_handle.cpp
		#source/allio/impl/posix/handles/packet_socket_handle.cpp
		source/allio/impl/posix/handles/stream_socket_handle.cpp
		source/allio/impl/win32/handles/event_handle.cpp
		source/allio/impl/win32/handles/filesystem_handle.cpp
		source/allio/impl/win32/handles/filesystem_handle.hpp
		source/allio/impl/win32/handles/platform_handle.cpp
		source/allio/impl/win32/handles/platform_handle.hpp
		source/allio/impl/win32/handles/process_handle.cpp
		source/allio/impl/win32/handles/process_handle.hpp

		source/allio/impl/win32/iocp/event_handle.cpp
		source/allio/impl/win32/iocp/generic_wait.cpp
		source/allio/impl/win32/iocp/listen_socket_handle.cpp
		source/allio/impl/win32/iocp/multiplexer.cpp
		source/allio/impl/win32/iocp/process_handle.cpp
		source/allio/impl/win32/iocp/stream_socket_handle.cpp

	TEST_SOURCES
		source/allio/test/win32/command_line.cpp
		source/allio/test/win32/kernel_path.cpp
		#source/allio/test/win32/timer2.cpp
		source/allio/test/win32/wait_packet.cpp
		source/allio/test/win32/wsa.cpp
)

vsm_configure(allio
	PLATFORM Linux
	
	HEADERS
		include/allio/linux/detail/io_uring/event_handle.hpp
		include/allio/linux/detail/io_uring/multiplexer.hpp
		include/allio/linux/platform.hpp
		include/allio/linux/timespec.hpp
		include/allio/linux/detail/path.hpp
		include/allio/linux/detail/path_view.hpp
		include/allio/linux/detail/undef.i
		include/allio/linux/detail/unique_fd.hpp
	
	SOURCES
		source/allio/impl/linux/kernel/fcntl.hpp
		source/allio/impl/linux/kernel/proc.hpp
		source/allio/impl/linux/kernel/stat.hpp
		source/allio/impl/linux/api_string.hpp
		source/allio/impl/linux/default_multiplexer.cpp
		#source/allio/impl/linux/directory_handle.cpp
		source/allio/impl/linux/error.cpp
		source/allio/impl/linux/error.hpp
		source/allio/impl/linux/event_handle.cpp
		#source/allio/impl/linux/filesystem_handle.cpp
		#source/allio/impl/linux/filesystem_handle.hpp
		#source/allio/impl/linux/file_handle.cpp
		#source/allio/impl/linux/map_handle.cpp
		source/allio/impl/linux/memory.cpp
		#source/allio/impl/linux/platform_handle.cpp
		#source/allio/impl/linux/platform_handle.hpp
		source/allio/impl/linux/platform.cpp
		#source/allio/impl/linux/posix_socket.cpp
		#source/allio/impl/linux/posix_socket.hpp
		#source/allio/impl/linux/process_handle.cpp
		#source/allio/impl/linux/process_handle.hpp
		#source/allio/impl/linux/section_handle.cpp
		#source/allio/impl/linux/sync_byte_io.cpp
		source/allio/impl/linux/unique_fd.cpp
		source/allio/impl/linux/unique_mmap.cpp
		source/allio/impl/linux/version.cpp
		source/allio/impl/linux/version.hpp

		source/allio/impl/linux/io_uring/event_handle.cpp
		source/allio/impl/linux/io_uring/multiplexer.cpp

	TEST_SOURCES
		source/allio/test/linux/io_uring.cpp
)


target_compile_definitions(allio_test
	PRIVATE
		"allio_detail_test_program=\"$<TARGET_FILE:test_program>\""
)
add_dependencies(allio_test test_program)

if(MSVC)
	target_compile_options(allio PUBLIC /Zc:preprocessor /Zc:__cplusplus)
	target_compile_definitions(allio PUBLIC _SILENCE_ALL_CXX23_DEPRECATION_WARNINGS)
endif()

add_subdirectory(tools)
