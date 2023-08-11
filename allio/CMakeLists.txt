cmake_minimum_required(VERSION 3.23.0)

project(allio)

include(FetchContent)

FetchContent_Declare(
	unifex
	#GIT_REPOSITORY https://github.com/facebookexperimental/libunifex.git
	#GIT_TAG main
	GIT_REPOSITORY https://github.com/vasama/libunifex.git
	GIT_TAG aligned_union
)
FetchContent_MakeAvailable(unifex)

find_package(vsm-box REQUIRED)
find_package(vsm-core REQUIRED)
find_package(vsm-flags REQUIRED)
find_package(vsm-linear REQUIRED)
find_package(vsm-intrusive REQUIRED)
find_package(vsm-result REQUIRED)
find_package(vsm-tag_ptr REQUIRED)
find_package(vsm-unique_resource REQUIRED)

vsm_add_library2(allio
	HEADERS
		include/allio/detail/any_handle.hpp
		include/allio/detail/api.hpp
		include/allio/detail/dynamic_buffer.hpp
		include/allio/detail/platform.h
		include/allio/detail/string_ref.hpp

		include/allio/async.hpp
		include/allio/async_fwd.hpp
		include/allio/async_result.hpp
		include/allio/byte_io.hpp
		include/allio/deadline.hpp
		include/allio/default_multiplexer.hpp
		include/allio/deferring_multiplexer.hpp
		include/allio/directory_handle.hpp
		include/allio/directory_handle_async.hpp
		include/allio/dynamic_storage.hpp
		include/allio/event_handle.hpp
		include/allio/filesystem.hpp
		include/allio/filesystem_handle.hpp
		include/allio/file_handle.hpp
		include/allio/file_handle_async.hpp
		include/allio/handle.hpp
		include/allio/implementation.hpp
		include/allio/input_path_view.hpp
		include/allio/input_string_view.hpp
		include/allio/manual_multiplexer.hpp
		include/allio/map_handle.hpp
		include/allio/multiplexer.hpp
		include/allio/network.hpp
		include/allio/output_path_ref.hpp
		include/allio/output_string_ref.hpp
		include/allio/packet_socket_handle.hpp
		include/allio/parameters.hpp
		include/allio/path.hpp
		include/allio/path_handle.hpp
		include/allio/path_literal.hpp
		include/allio/path_view.hpp
		include/allio/platform_handle.hpp
		include/allio/process_handle.hpp
		include/allio/section_handle.hpp
		include/allio/socket_handle.hpp
		include/allio/socket_handle_async.hpp
		include/allio/stream_socket_handle.hpp
		include/allio/step_deadline.hpp
		include/allio/storage.hpp
		include/allio/synchronized_multiplexer.hpp
		include/allio/sync_executor_multiplexer.hpp
		include/allio/sync_wait.hpp
		include/allio/type_id.hpp
		include/allio/type_list.hpp
		include/allio/untyped_parameter.hpp

		include/allio/linux/io_uring_multiplexer.hpp
		include/allio/linux/platform.hpp
		include/allio/linux/timeout.hpp
		include/allio/linux/detail/path.hpp
		include/allio/linux/detail/path_view.hpp
		include/allio/linux/detail/undef.i
		include/allio/linux/detail/unique_fd.hpp

		include/allio/win32/iocp_multiplexer.hpp
		include/allio/win32/ioring_multiplexer.hpp
		include/allio/win32/nt_error.hpp
		include/allio/win32/platform.hpp
		include/allio/win32/detail/path.hpp
		include/allio/win32/detail/path_view.hpp
		include/allio/win32/detail/unique_handle.hpp
		include/allio/win32/detail/win32_fwd.hpp

	SOURCES
		source/allio/impl/async_handle_types.hpp
		source/allio/impl/default_multiplexer.cpp
		source/allio/impl/deferring_multiplexer.cpp
		source/allio/impl/directory_handle.cpp
		source/allio/impl/error.cpp
		source/allio/impl/event_handle.cpp
		source/allio/impl/filesystem_handle.cpp
		source/allio/impl/file_handle.cpp
		source/allio/impl/handle.cpp
		source/allio/impl/implementation.cpp
		source/allio/impl/manual_multiplexer.cpp
		source/allio/impl/map_handle.cpp
		source/allio/impl/multiplexer.cpp
		source/allio/impl/object_transaction.hpp
		source/allio/impl/platform_handle.cpp
		source/allio/impl/posix_socket.cpp
		source/allio/impl/posix_socket.hpp
		source/allio/impl/process_handle.cpp
		source/allio/impl/section_handle.cpp
		source/allio/impl/socket_handle.cpp
		source/allio/impl/sync_byte_io.hpp
		source/allio/impl/unique_name.cpp
		source/allio/impl/unique_name.hpp

	HEADER_DEPENDENCIES
		vsm::box
		vsm::core
		vsm::flags
		vsm::linear
		vsm::intrusive
		vsm::result
		vsm::tag_ptr
		vsm::unique_resource
		unifex

	TEST_SOURCES
		source/allio/test/directory_handle.cpp
		source/allio/test/event_handle.cpp
		source/allio/test/file_handle.cpp
		source/allio/test/filesystem.hpp
		source/allio/test/map_handle.cpp
		source/allio/test/multiplexer.hpp
		source/allio/test/path_view.cpp
		source/allio/test/process_handle.cpp
		source/allio/test/reverse_iterator.hpp
		source/allio/test/signal.hpp
		source/allio/test/socket_handle.cpp
)

vsm_configure(allio
	PLATFORM Windows

	SOURCES
		source/allio/impl/win32/default_multiplexer.cpp
		source/allio/impl/win32/directory_handle.cpp
		source/allio/impl/win32/directory_handle.hpp
		source/allio/impl/win32/encoding.cpp
		source/allio/impl/win32/encoding.hpp
		source/allio/impl/win32/error.hpp
		source/allio/impl/win32/event_handle.cpp
		source/allio/impl/win32/filesystem_handle.cpp
		source/allio/impl/win32/filesystem_handle.hpp
		source/allio/impl/win32/file_handle.cpp
		source/allio/impl/win32/iocp_byte_io.hpp
		source/allio/impl/win32/iocp_event_handle.cpp
		source/allio/impl/win32/iocp_directory_handle.cpp
		source/allio/impl/win32/iocp_filesystem_handle.hpp
		source/allio/impl/win32/iocp_file_handle.cpp
		source/allio/impl/win32/iocp_multiplexer.cpp
		source/allio/impl/win32/iocp_platform_handle.hpp
		source/allio/impl/win32/iocp_process_handle.cpp
		source/allio/impl/win32/iocp_socket_handle.cpp
		source/allio/impl/win32/kernel.cpp
		source/allio/impl/win32/kernel.hpp
		source/allio/impl/win32/kernel_path.cpp
		source/allio/impl/win32/kernel_path.hpp
		source/allio/impl/win32/kernel_path_impl.hpp
		source/allio/impl/win32/kernel_path_old.hpp
		source/allio/impl/win32/map_handle.cpp
		source/allio/impl/win32/nt_error.cpp
		source/allio/impl/win32/peb.cpp
		source/allio/impl/win32/peb.hpp
		source/allio/impl/win32/platform_handle.cpp
		source/allio/impl/win32/platform_handle.hpp
		source/allio/impl/win32/posix_socket.cpp
		source/allio/impl/win32/posix_socket.hpp
		source/allio/impl/win32/process_handle.cpp
		source/allio/impl/win32/process_handle.hpp
		source/allio/impl/win32/section_handle.cpp
		source/allio/impl/win32/sync_byte_io.cpp
		source/allio/impl/win32/sync_filesystem_handle.hpp
		source/allio/impl/win32/unique_handle.cpp
		source/allio/impl/win32/wsa_ex.cpp
		source/allio/impl/win32/wsa_ex.hpp

	TEST_SOURCES
		source/allio/test/win32/kernel_path.cpp
		source/allio/test/win32/signal.cpp
)

vsm_configure(allio
	PLATFORM Linux

	SOURCES
		source/allio/impl/linux/api_string.hpp
		source/allio/impl/linux/default_multiplexer.cpp
		#source/allio/impl/linux/directory_handle.cpp
		source/allio/impl/linux/error.hpp
		source/allio/impl/linux/event_handle.cpp
		source/allio/impl/linux/filesystem_handle.cpp
		source/allio/impl/linux/filesystem_handle.hpp
		source/allio/impl/linux/file_handle.cpp
		source/allio/impl/linux/io_uring_byte_io.hpp
		source/allio/impl/linux/io_uring_event_handle.cpp
		source/allio/impl/linux/io_uring_filesystem_handle.hpp
		source/allio/impl/linux/io_uring_file_handle.cpp
		source/allio/impl/linux/io_uring_multiplexer.cpp
		source/allio/impl/linux/io_uring_platform_handle.hpp
		source/allio/impl/linux/io_uring_process_handle.cpp
		source/allio/impl/linux/io_uring_socket_handle.cpp
		source/allio/impl/linux/map_handle.cpp
		source/allio/impl/linux/platform_handle.cpp
		source/allio/impl/linux/platform_handle.hpp
		source/allio/impl/linux/posix_socket.cpp
		source/allio/impl/linux/posix_socket.hpp
		source/allio/impl/linux/process_handle.cpp
		source/allio/impl/linux/process_handle.hpp
		source/allio/impl/linux/section_handle.cpp
		source/allio/impl/linux/stat.hpp
		source/allio/impl/linux/sync_byte_io.cpp
		source/allio/impl/linux/sync_filesystem_handle.hpp
		source/allio/impl/linux/unique_fd.cpp
		source/allio/impl/linux/version.cpp
		source/allio/impl/linux/version.hpp

	TEST_SOURCES
		source/allio/test/linux/signal.cpp
)

if(DEFINE NOT_DEFINED)
vsm_target_platform_sources(allio Windows
	source/allio/impl/win32/default_multiplexer.cpp
	source/allio/impl/win32/directory_handle.cpp
	source/allio/impl/win32/directory_handle.hpp
	source/allio/impl/win32/encoding.cpp
	source/allio/impl/win32/encoding.hpp
	source/allio/impl/win32/error.hpp
	source/allio/impl/win32/event_handle.cpp
	source/allio/impl/win32/filesystem_handle.cpp
	source/allio/impl/win32/filesystem_handle.hpp
	source/allio/impl/win32/file_handle.cpp
	source/allio/impl/win32/iocp_byte_io.hpp
	source/allio/impl/win32/iocp_event_handle.cpp
	source/allio/impl/win32/iocp_directory_handle.cpp
	source/allio/impl/win32/iocp_filesystem_handle.hpp
	source/allio/impl/win32/iocp_file_handle.cpp
	source/allio/impl/win32/iocp_multiplexer.cpp
	source/allio/impl/win32/iocp_platform_handle.hpp
	source/allio/impl/win32/iocp_process_handle.cpp
	source/allio/impl/win32/iocp_socket_handle.cpp
	source/allio/impl/win32/kernel.cpp
	source/allio/impl/win32/kernel.hpp
	source/allio/impl/win32/kernel_path.cpp
	source/allio/impl/win32/kernel_path.hpp
	source/allio/impl/win32/kernel_path_impl.hpp
	source/allio/impl/win32/kernel_path_old.hpp
	source/allio/impl/win32/map_handle.cpp
	source/allio/impl/win32/nt_error.cpp
	source/allio/impl/win32/peb.cpp
	source/allio/impl/win32/peb.hpp
	source/allio/impl/win32/platform_handle.cpp
	source/allio/impl/win32/platform_handle.hpp
	source/allio/impl/win32/posix_socket.cpp
	source/allio/impl/win32/posix_socket.hpp
	source/allio/impl/win32/process_handle.cpp
	source/allio/impl/win32/process_handle.hpp
	source/allio/impl/win32/section_handle.cpp
	source/allio/impl/win32/sync_byte_io.cpp
	source/allio/impl/win32/sync_filesystem_handle.hpp
	source/allio/impl/win32/unique_handle.cpp
	source/allio/impl/win32/wsa_ex.cpp
	source/allio/impl/win32/wsa_ex.hpp
)

vsm_target_platform_sources(allio Linux
	source/allio/impl/linux/api_string.hpp
	source/allio/impl/linux/default_multiplexer.cpp
	#source/allio/impl/linux/directory_handle.cpp
	source/allio/impl/linux/error.hpp
	source/allio/impl/linux/event_handle.cpp
	source/allio/impl/linux/filesystem_handle.cpp
	source/allio/impl/linux/filesystem_handle.hpp
	source/allio/impl/linux/file_handle.cpp
	source/allio/impl/linux/io_uring_byte_io.hpp
	source/allio/impl/linux/io_uring_event_handle.cpp
	source/allio/impl/linux/io_uring_filesystem_handle.hpp
	source/allio/impl/linux/io_uring_file_handle.cpp
	source/allio/impl/linux/io_uring_multiplexer.cpp
	source/allio/impl/linux/io_uring_platform_handle.hpp
	source/allio/impl/linux/io_uring_process_handle.cpp
	source/allio/impl/linux/io_uring_socket_handle.cpp
	source/allio/impl/linux/map_handle.cpp
	source/allio/impl/linux/platform_handle.cpp
	source/allio/impl/linux/platform_handle.hpp
	source/allio/impl/linux/posix_socket.cpp
	source/allio/impl/linux/posix_socket.hpp
	source/allio/impl/linux/process_handle.cpp
	source/allio/impl/linux/process_handle.hpp
	source/allio/impl/linux/section_handle.cpp
	source/allio/impl/linux/stat.hpp
	source/allio/impl/linux/sync_byte_io.cpp
	source/allio/impl/linux/sync_filesystem_handle.hpp
	source/allio/impl/linux/unique_fd.cpp
	source/allio/impl/linux/version.cpp
	source/allio/impl/linux/version.hpp
)
endif()

target_compile_definitions(allio_test
	PRIVATE
		"allio_detail_add_numbers_path=\"$<TARGET_FILE:add_numbers>\""
)
add_dependencies(allio_test add_numbers)

if(MSVC)
	target_compile_options(allio PUBLIC /Zc:preprocessor)
	target_compile_definitions(allio PUBLIC _SILENCE_ALL_CXX23_DEPRECATION_WARNINGS)
endif()

target_include_directories(allio PUBLIC D:/Code/stdexec/include)
#target_compile_definitions(allio PUBLIC allio_config_unifex=1)
target_compile_definitions(allio PUBLIC _SILENCE_CXX23_ALIGNED_UNION_DEPRECATION_WARNING)

add_subdirectory(tools)
