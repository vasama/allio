cmake_minimum_required(VERSION 3.24)
project(allio_core)

find_package(vsm.cmake REQUIRED)
vsm_define_package(allio.core)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/add_error_encoding.cmake")

try_compile(
	has_std_stacktrace
	SOURCES "${CMAKE_CURRENT_LIST_DIR}/tools/detect-std-stacktrace.cpp")

vsm_add_library(
	allio::core

	HEADERS
		include/allio/byte_io.hpp
		include/allio/byte_io_buffers.hpp
		include/allio/deadline.hpp
		include/allio/filesystem.hpp
		include/allio/handle.hpp
		include/allio/mapped.hpp
		include/allio/network.hpp
		include/allio/path_literals.hpp
		include/allio/path_view.hpp
		include/allio/path.hpp
		include/allio/step_deadline.hpp

		include/allio/detail/api.hpp
		include/allio/detail/byte_io.hpp
		include/allio/detail/byte_io_buffers.hpp
		include/allio/detail/dynamic_buffer.hpp
		include/allio/detail/platform.h
		include/allio/detail/type_list.hpp
		include/allio/detail/unique_handle.hpp

		# Handles
		include/allio/handles/datagram_socket_base.hpp
		include/allio/handles/directory.hpp
		include/allio/handles/event.hpp
		include/allio/handles/file.hpp
		include/allio/handles/fs_object.hpp
		include/allio/handles/listen_socket_base.hpp
		include/allio/handles/map.hpp
		include/allio/handles/opaque_object.hpp
		include/allio/handles/platform_object.hpp
		include/allio/handles/process.hpp
		include/allio/handles/raw_datagram_socket.hpp
		include/allio/handles/raw_listen_socket.hpp
		include/allio/handles/raw_socket.hpp
		include/allio/handles/section.hpp
		include/allio/handles/socket_base.hpp

		include/allio/detail/handles/directory.hpp
		include/allio/detail/handles/event.hpp
		include/allio/detail/handles/file.hpp
		include/allio/detail/handles/fs_object.hpp
		include/allio/detail/handles/map.hpp
		include/allio/detail/handles/opaque_object.hpp
		include/allio/detail/handles/platform_object.hpp
		include/allio/detail/handles/process.hpp
		include/allio/detail/handles/raw_datagram_socket.hpp
		include/allio/detail/handles/raw_listen_socket.hpp
		include/allio/detail/handles/raw_socket.hpp
		include/allio/detail/handles/section.hpp

	SOURCES
		source/allio/impl/any_byte_buffer.cpp
		source/allio/impl/buffer_registrar.cpp
		source/allio/impl/byte_io_buffers.cpp
		source/allio/impl/error_default.cpp
		source/allio/impl/error.cpp
		source/allio/impl/fd_tree.hpp
		source/allio/impl/network.cpp
		source/allio/impl/new.cpp
		source/allio/impl/random.hpp
		source/allio/impl/serialization.cpp
		source/allio/impl/transcode.hpp
		source/allio/impl/unique_name.cpp
		source/allio/impl/unique_name.hpp

		# Handles
		source/allio/impl/handles/directory.cpp
		source/allio/impl/handles/event.cpp
		source/allio/impl/handles/file.cpp
		source/allio/impl/handles/fs_object.cpp
		source/allio/impl/handles/fs_object.hpp
		source/allio/impl/handles/map.cpp
		source/allio/impl/handles/object.cpp
		source/allio/impl/handles/opaque_object.cpp
		#source/allio/impl/handles/path.cpp
		source/allio/impl/handles/pipe.cpp
		source/allio/impl/handles/platform_object.hpp
		source/allio/impl/handles/process.cpp
		source/allio/impl/handles/section.cpp
		source/allio/impl/handles/standard_stream.cpp
		#source/allio/impl/handles/thread.cpp

	SOURCE_DEFINITIONS
		"allio_config_std_stacktrace=${has_std_stacktrace}"

	HEADER_DEPENDENCIES
		vsm::core
		vsm::result
		vsm::tag_ptr
		vsm::unique_resource

	TEST_SOURCES
		source/allio/test/reverse_iterator.hpp

		source/allio/test/byte_io_buffers.cpp
		source/allio/test/fd_tree.cpp
		source/allio/test/network.cpp
		source/allio/test/path_view.cpp
)

vsm_configure(
	allio::core
	PLATFORM Windows

	HEADERS
		include/allio/win32/kernel_error.hpp

		include/allio/win32/detail/path_char.hpp
		include/allio/win32/detail/path_view.hpp
		include/allio/win32/detail/win32_fwd.hpp

		# iocp
		include/allio/win32/detail/iocp/directory.hpp
		include/allio/win32/detail/iocp/event.hpp
		include/allio/win32/detail/iocp/file.hpp
		include/allio/win32/detail/iocp/multiplexer.hpp
		include/allio/win32/detail/iocp/opaque_object.hpp
		include/allio/win32/detail/iocp/process.hpp
		include/allio/win32/detail/iocp/raw_datagram_socket.hpp
		include/allio/win32/detail/iocp/raw_listen_socket.hpp
		include/allio/win32/detail/iocp/raw_socket.hpp
		include/allio/win32/detail/iocp/wait.hpp

	SOURCES
		source/allio/impl/posix/socket.cpp
		source/allio/impl/posix/socket.hpp
		source/allio/impl/posix/unique_socket.cpp
		source/allio/impl/win32/api_string.cpp
		source/allio/impl/win32/api_string.hpp
		source/allio/impl/win32/byte_io.cpp
		source/allio/impl/win32/byte_io.hpp
		source/allio/impl/win32/command_line.cpp
		source/allio/impl/win32/command_line.hpp
		source/allio/impl/win32/completion_port.cpp
		source/allio/impl/win32/completion_port.hpp
		source/allio/impl/win32/environment_block.cpp
		source/allio/impl/win32/environment_block.hpp
		source/allio/impl/win32/error.cpp
		source/allio/impl/win32/error.hpp
		source/allio/impl/win32/event.cpp
		source/allio/impl/win32/event.hpp
		source/allio/impl/win32/kernel_error.cpp
		source/allio/impl/win32/kernel_path_impl.hpp
		source/allio/impl/win32/kernel_path.cpp
		source/allio/impl/win32/kernel_path.hpp
		source/allio/impl/win32/kernel.cpp
		source/allio/impl/win32/kernel.hpp
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
		source/allio/impl/win32/thread_event.cpp
		source/allio/impl/win32/thread_event.hpp
		source/allio/impl/win32/transcode.cpp
		source/allio/impl/win32/wait_packet.cpp
		source/allio/impl/win32/wait_packet.hpp
		source/allio/impl/win32/wsa.cpp
		source/allio/impl/win32/wsa.hpp

		# Handles
		source/allio/impl/win32/handles/raw_datagram_socket.cpp
		source/allio/impl/win32/handles/directory.cpp
		source/allio/impl/win32/handles/event.cpp
		source/allio/impl/win32/handles/file.cpp
		source/allio/impl/win32/handles/fs_object.cpp
		source/allio/impl/win32/handles/fs_object.hpp
		source/allio/impl/win32/handles/map.cpp
		source/allio/impl/win32/handles/opaque_object.cpp
		source/allio/impl/win32/handles/pipe.cpp
		source/allio/impl/win32/handles/platform_object.cpp
		source/allio/impl/win32/handles/platform_object.hpp
		source/allio/impl/win32/handles/process.cpp
		source/allio/impl/win32/handles/process.hpp
		source/allio/impl/win32/handles/raw_listen_socket.cpp
		source/allio/impl/win32/handles/raw_socket.cpp
		source/allio/impl/win32/handles/section.cpp
		source/allio/impl/win32/handles/standard_stream.cpp

		# IOCP
		source/allio/impl/win32/iocp/byte_io.cpp
		source/allio/impl/win32/iocp/directory.cpp
		source/allio/impl/win32/iocp/event.cpp
		source/allio/impl/win32/iocp/file.cpp
		source/allio/impl/win32/iocp/multiplexer.cpp
		source/allio/impl/win32/iocp/pipe.cpp
		source/allio/impl/win32/iocp/process.cpp
		source/allio/impl/win32/iocp/raw_datagram_socket.cpp
		source/allio/impl/win32/iocp/raw_listen_socket.cpp
		source/allio/impl/win32/iocp/raw_socket.cpp
		source/allio/impl/win32/iocp/raw_socket.hpp
		source/allio/impl/win32/iocp/wait.cpp

	HEADER_DEPENDENCIES
		vsm::intrusive_ptr

	SOURCE_DEPENDENCIES
		vsm::intrusive

	TEST_SOURCES
		#source/allio/test/win32/environment_block.cpp
		#source/allio/test/win32/timer2.cpp
		source/allio/test/win32/command_line.cpp
		source/allio/test/win32/completion_port.cpp
		source/allio/test/win32/completion_port.hpp
		source/allio/test/win32/directory.cpp
		source/allio/test/win32/kernel_path.cpp
		source/allio/test/win32/pipe.cpp
		source/allio/test/win32/wait_packet.cpp
		source/allio/test/win32/wsa.cpp
)

vsm_configure(
	allio::core
	PLATFORM Linux

	HEADERS
		include/allio/linux/timespec.hpp

		include/allio/linux/detail/path_char.hpp
		include/allio/linux/detail/path_view.hpp
		include/allio/linux/detail/undef.i

		# io_uring
		include/allio/linux/detail/io_uring/directory.hpp
		include/allio/linux/detail/io_uring/event.hpp
		include/allio/linux/detail/io_uring/file.hpp
		include/allio/linux/detail/io_uring/io_uring.hpp
		include/allio/linux/detail/io_uring/multiplexer.hpp
		include/allio/linux/detail/io_uring/opaque_object.hpp
		include/allio/linux/detail/io_uring/process.hpp
		include/allio/linux/detail/io_uring/raw_datagram_socket.hpp
		include/allio/linux/detail/io_uring/raw_listen_socket.hpp
		include/allio/linux/detail/io_uring/raw_socket.hpp

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
		source/allio/impl/linux/poll.hpp
		source/allio/impl/linux/posix_socket.cpp
		source/allio/impl/linux/posix_socket.hpp
		source/allio/impl/linux/proc.hpp
		source/allio/impl/linux/process_reaper.cpp
		source/allio/impl/linux/process_reaper.hpp
		source/allio/impl/linux/process.cpp
		source/allio/impl/linux/process.hpp
		source/allio/impl/linux/random.cpp
		source/allio/impl/linux/readlink.cpp
		source/allio/impl/linux/readlink.hpp
		source/allio/impl/linux/signal.cpp
		source/allio/impl/linux/signal.hpp
		source/allio/impl/linux/stat.hpp
		source/allio/impl/linux/timeout.hpp
		source/allio/impl/linux/transcode.cpp
		source/allio/impl/linux/version.cpp
		source/allio/impl/linux/version.hpp
		source/allio/impl/posix/socket.cpp
		source/allio/impl/posix/socket.hpp
		source/allio/impl/posix/unique_socket.cpp

		# Handles
		source/allio/impl/linux/handles/directory.cpp
		source/allio/impl/linux/handles/event.cpp
		source/allio/impl/linux/handles/event.hpp
		source/allio/impl/linux/handles/file.cpp
		source/allio/impl/linux/handles/fs_object.cpp
		source/allio/impl/linux/handles/fs_object.hpp
		source/allio/impl/linux/handles/map.cpp
		source/allio/impl/linux/handles/opaque_object.cpp
		source/allio/impl/linux/handles/pipe.cpp
		source/allio/impl/linux/handles/platform_object.cpp
		source/allio/impl/linux/handles/process.cpp
		source/allio/impl/linux/handles/section.cpp
		source/allio/impl/linux/handles/standard_stream.cpp
		source/allio/impl/posix/handles/raw_datagram_socket.cpp
		source/allio/impl/posix/handles/raw_listen_socket.cpp
		source/allio/impl/posix/handles/raw_socket.cpp

		# io_uring
		source/allio/impl/linux/io_uring/byte_io.cpp
		#source/allio/impl/linux/io_uring/process.cpp
		source/allio/impl/linux/io_uring/event.cpp
		source/allio/impl/linux/io_uring/multiplexer.cpp
		source/allio/impl/linux/io_uring/pipe.cpp
		source/allio/impl/linux/io_uring/raw_datagram_socket.cpp
		source/allio/impl/linux/io_uring/raw_listen_socket.cpp
		source/allio/impl/linux/io_uring/raw_socket.cpp

	TEST_SOURCES
		source/allio/test/linux/io_uring.cpp

	SOURCE_DEPENDENCIES
		vsm::intrusive
)

allio_add_error_encoding(core core_error_encoding allio::core)
vsm_configure(allio::core INTERNAL_DEPENDENCIES core_error_encoding)
