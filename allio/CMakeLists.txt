cmake_minimum_required(VERSION 3.23)

project(allio-core)

find_package(vsm-box REQUIRED)
find_package(vsm-core REQUIRED)
find_package(vsm-flags REQUIRED)
find_package(vsm-intrusive REQUIRED)
find_package(vsm-intrusive_ptr REQUIRED)
find_package(vsm-linear REQUIRED)
find_package(vsm-literals REQUIRED)
find_package(vsm-math REQUIRED)
find_package(vsm-numeric REQUIRED)
find_package(vsm-offset_ptr REQUIRED)
find_package(vsm-optional REQUIRED)
find_package(vsm-result REQUIRED)
find_package(vsm-tag_ptr REQUIRED)
find_package(vsm-unique_resource REQUIRED)
find_package(p2300 REQUIRED)

vsm_add_library(allio::core
	HEADERS
		include/allio/async_result.hpp
		include/allio/byte_io.hpp
		include/allio/deadline.hpp
		include/allio/default_multiplexer.hpp
		include/allio/directory.hpp
		include/allio/event.hpp
		include/allio/filesystem.hpp
		include/allio/fs_object.hpp
		include/allio/file.hpp
		include/allio/handle.hpp
		include/allio/map.hpp
		include/allio/network.hpp
		include/allio/path.hpp
		include/allio/path_literals.hpp
		include/allio/path_view.hpp
		include/allio/platform_object.hpp
		include/allio/process.hpp
		include/allio/section.hpp
		include/allio/socket.hpp
		include/allio/step_deadline.hpp
		include/allio/sync_wait.hpp

		include/allio/handles/datagram_socket.hpp
		include/allio/handles/directory.hpp
		include/allio/handles/event.hpp
		include/allio/handles/file.hpp
		include/allio/handles/fs_object.hpp
		include/allio/handles/listen_socket.hpp
		include/allio/handles/map.hpp
		include/allio/handles/opaque_object.hpp
		include/allio/handles/platform_object.hpp
		include/allio/handles/process.hpp
		include/allio/handles/section.hpp
		include/allio/handles/socket.hpp

		include/allio/detail/any_handle.hpp
		include/allio/detail/api.hpp
		include/allio/detail/dynamic_buffer.hpp
		include/allio/detail/platform.h
		include/allio/detail/string_ref.hpp
		include/allio/detail/type_list.hpp
		include/allio/detail/unique_handle.hpp

		include/allio/detail/handles/datagram_socket.hpp
		include/allio/detail/handles/directory.hpp
		include/allio/detail/handles/event.hpp
		include/allio/detail/handles/file.hpp
		include/allio/detail/handles/fs_object.hpp
		include/allio/detail/handles/listen_socket.hpp
		include/allio/detail/handles/map.hpp
		include/allio/detail/handles/opaque_object.hpp
		include/allio/detail/handles/platform_object.hpp
		include/allio/detail/handles/process.hpp
		include/allio/detail/handles/section.hpp
		include/allio/detail/handles/socket.hpp

	SOURCES
		source/allio/impl/error.cpp
		source/allio/impl/network.cpp
		source/allio/impl/random.hpp
		source/allio/impl/transcode.hpp
		source/allio/impl/unique_name.cpp
		source/allio/impl/unique_name.hpp

		source/allio/impl/handles/fs_object.cpp
		source/allio/impl/handles/fs_object.hpp

	HEADER_DEPENDENCIES
		vsm::box
		vsm::core
		vsm::flags
		vsm::intrusive
		vsm::intrusive_ptr
		vsm::linear
		vsm::literals
		vsm::math
		vsm::numeric
		vsm::offset_ptr
		vsm::optional
		vsm::result
		vsm::tag_ptr
		vsm::unique_resource
		p2300::p2300

	TEST_SOURCES
		source/allio/test/filesystem.hpp
		source/allio/test/io_result.cpp
		source/allio/test/multiplexer.hpp
		source/allio/test/memory.cpp
		source/allio/test/network.cpp
		source/allio/test/path_view.cpp
		source/allio/test/reverse_iterator.hpp
		source/allio/test/sync_wait.cpp
		source/allio/test/task.hpp

		source/allio/test/handles/datagram_socket.cpp
		source/allio/test/handles/directory.cpp
		source/allio/test/handles/event.cpp
		source/allio/test/handles/file.cpp
		source/allio/test/handles/map.cpp
		#source/allio/test/handles/opaque_object.cpp
		source/allio/test/handles/process.cpp
		source/allio/test/handles/socket.cpp
)

vsm_configure(allio::core
	PLATFORM Windows
	
	HEADERS
		include/allio/win32/kernel_error.hpp
		include/allio/win32/detail/path.hpp
		include/allio/win32/detail/path_view.hpp
		include/allio/win32/detail/win32_fwd.hpp

		# iocp
		include/allio/win32/detail/iocp/byte_io.hpp
		include/allio/win32/detail/iocp/event.hpp
		include/allio/win32/detail/iocp/multiplexer.hpp
		include/allio/win32/detail/iocp/wait.hpp

	SOURCES
		source/allio/impl/posix/socket.cpp
		source/allio/impl/posix/socket.hpp
		source/allio/impl/posix/unique_socket.cpp
		source/allio/impl/win32/api_string.cpp
		source/allio/impl/win32/api_string.hpp
		source/allio/impl/win32/byte_io.cpp
		source/allio/impl/win32/byte_io.hpp
		source/allio/impl/win32/completion_port.cpp
		source/allio/impl/win32/completion_port.hpp
		source/allio/impl/win32/command_line.cpp
		source/allio/impl/win32/command_line.hpp
		source/allio/impl/win32/environment_block.cpp
		source/allio/impl/win32/environment_block.hpp
		source/allio/impl/win32/error.cpp
		source/allio/impl/win32/error.hpp
		source/allio/impl/win32/kernel.cpp
		source/allio/impl/win32/kernel.hpp
		source/allio/impl/win32/kernel_error.cpp
		source/allio/impl/win32/kernel_path.cpp
		source/allio/impl/win32/kernel_path.hpp
		source/allio/impl/win32/kernel_path_impl.hpp
		source/allio/impl/win32/memory.cpp
		source/allio/impl/win32/path.cpp
		source/allio/impl/win32/path.hpp
		source/allio/impl/win32/peb.cpp
		source/allio/impl/win32/peb.hpp
		source/allio/impl/win32/platform.cpp
		source/allio/impl/win32/posix_socket.cpp
		source/allio/impl/win32/posix_socket.hpp
		source/allio/impl/win32/random.cpp
		source/allio/impl/win32/secrets.cpp
		source/allio/impl/win32/transcode.cpp
		source/allio/impl/win32/wait_packet.cpp
		source/allio/impl/win32/wait_packet.hpp
		source/allio/impl/win32/wsa.cpp
		source/allio/impl/win32/wsa.hpp

		# handles
		source/allio/impl/posix/handles/datagram_socket.cpp
		source/allio/impl/posix/handles/listen_socket.cpp
		source/allio/impl/posix/handles/socket.cpp
		source/allio/impl/win32/handles/directory.cpp
		source/allio/impl/win32/handles/event.cpp
		source/allio/impl/win32/handles/file.cpp
		source/allio/impl/win32/handles/fs_object.cpp
		source/allio/impl/win32/handles/fs_object.hpp
		source/allio/impl/win32/handles/map.cpp
		source/allio/impl/win32/handles/pipe.cpp
		source/allio/impl/win32/handles/platform_object.cpp
		source/allio/impl/win32/handles/platform_object.hpp
		source/allio/impl/win32/handles/process.cpp
		source/allio/impl/win32/handles/process.hpp
		source/allio/impl/win32/handles/section.cpp

		# iocp
		source/allio/impl/win32/iocp/datagram_socket.cpp
		source/allio/impl/win32/iocp/directory.cpp
		source/allio/impl/win32/iocp/event.cpp
		source/allio/impl/win32/iocp/file.cpp
		source/allio/impl/win32/iocp/listen_socket.cpp
		source/allio/impl/win32/iocp/multiplexer.cpp
		source/allio/impl/win32/iocp/process.cpp
		source/allio/impl/win32/iocp/socket.cpp
		source/allio/impl/win32/iocp/wait.cpp

	TEST_SOURCES
		source/allio/test/win32/afd.cpp
		source/allio/test/win32/command_line.cpp
		source/allio/test/win32/kernel_path.cpp
		source/allio/test/win32/memory.cpp
		#source/allio/test/win32/timer2.cpp
		source/allio/test/win32/wait_packet.cpp
		source/allio/test/win32/wsa.cpp
)

vsm_configure(allio::core
	PLATFORM Linux
	
	HEADERS
		include/allio/linux/detail/path.hpp
		include/allio/linux/detail/path_view.hpp
		include/allio/linux/detail/undef.i
		include/allio/linux/platform.hpp
		include/allio/linux/timespec.hpp

		# io_uring
		include/allio/linux/detail/io_uring/event.hpp
		include/allio/linux/detail/io_uring/listen_socket.hpp
		include/allio/linux/detail/io_uring/multiplexer.hpp
		include/allio/linux/detail/io_uring/socket.hpp
	
	SOURCES
		source/allio/impl/linux/api_string.hpp
		source/allio/impl/linux/byte_io.cpp
		source/allio/impl/linux/byte_io.hpp
		source/allio/impl/linux/error.cpp
		source/allio/impl/linux/error.hpp
		source/allio/impl/linux/fcntl.hpp
		source/allio/impl/linux/fork_exec.cpp
		source/allio/impl/linux/fork_exec.hpp
		source/allio/impl/linux/io_uring.hpp
		source/allio/impl/linux/memory.cpp
		source/allio/impl/linux/mman.hpp
		source/allio/impl/linux/mmap.cpp
		source/allio/impl/linux/platform.cpp
		source/allio/impl/linux/posix_socket.hpp
		source/allio/impl/linux/posix_socket.cpp
		source/allio/impl/linux/proc.hpp
		source/allio/impl/linux/process_reaper.hpp
		source/allio/impl/linux/process_reaper.cpp
		source/allio/impl/linux/random.cpp
		source/allio/impl/linux/stat.hpp
		source/allio/impl/linux/timeout.hpp
		source/allio/impl/linux/transcode.cpp
		source/allio/impl/linux/unique_mmap.cpp
		source/allio/impl/linux/version.cpp
		source/allio/impl/linux/version.hpp
		source/allio/impl/posix/socket.cpp
		source/allio/impl/posix/socket.hpp
		source/allio/impl/posix/unique_socket.cpp

		# handles
		source/allio/impl/linux/handles/directory.cpp
		source/allio/impl/linux/handles/event.cpp
		source/allio/impl/linux/handles/event.hpp
		source/allio/impl/linux/handles/file.cpp
		source/allio/impl/linux/handles/fs_object.cpp
		source/allio/impl/linux/handles/fs_object.hpp
		source/allio/impl/linux/handles/map.cpp
		source/allio/impl/linux/handles/pipe.cpp
		source/allio/impl/linux/handles/platform_object.cpp
		source/allio/impl/linux/handles/process.cpp
		source/allio/impl/linux/handles/section.cpp
		source/allio/impl/posix/handles/datagram_socket.cpp
		source/allio/impl/posix/handles/listen_socket.cpp
		source/allio/impl/posix/handles/socket.cpp

		# io_uring
		source/allio/impl/linux/io_uring/datagram_socket.cpp
		source/allio/impl/linux/io_uring/event.cpp
		source/allio/impl/linux/io_uring/listen_socket.cpp
		source/allio/impl/linux/io_uring/multiplexer.cpp
		source/allio/impl/linux/io_uring/socket.cpp
		#source/allio/impl/linux/io_uring/process.cpp

	TEST_SOURCES
		source/allio/test/linux/io_uring.cpp
		source/allio/test/linux/memory.cpp
)


set(allio_test_program_output "${CMAKE_CURRENT_BINARY_DIR}/test_program_output.txt")

target_compile_definitions(allio_core_test
	PRIVATE
		"allio_detail_test_program=\"$<TARGET_FILE:test_program>\""
		"allio_detail_test_program_output=\"${allio_test_program_output}\""
)
add_dependencies(allio_core_test test_program)

add_subdirectory(tools)
