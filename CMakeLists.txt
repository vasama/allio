cmake_minimum_required(VERSION 3.21)

project(allio)

set(CMAKE_CXX_STANDARD 17)


include(FetchContent)

FetchContent_Declare(
	Catch2
	GIT_REPOSITORY https://github.com/catchorg/Catch2.git
	GIT_TAG v3.1.0
)
FetchContent_MakeAvailable(Catch2)

FetchContent_Declare(
	tl_expected
	GIT_REPOSITORY https://github.com/vasama/expected.git
	GIT_TAG master
)
FetchContent_MakeAvailable(tl_expected)

FetchContent_Declare(
	unifex
	GIT_REPOSITORY https://github.com/facebookexperimental/libunifex.git
	GIT_TAG main
)
FetchContent_MakeAvailable(unifex)

#FetchContent_Declare(
#	stdexec
#	GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
#	GIT_TAG main
#)
#FetchContent_MakeAvailable(stdexec)


if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()


function(add_fuzz_test target source)
	add_executable(${target}
		${source}
	)
	target_compile_options(${target}
		PRIVATE "/fsanitize=fuzzer"
	)
endfunction()


add_library(allio
	source/allio/impl/directory_handle.cpp
	source/allio/impl/default_multiplexer.cpp
	source/allio/impl/deferring_multiplexer.cpp
	source/allio/impl/file_handle.cpp
	source/allio/impl/filesystem_handle.cpp
	source/allio/impl/handle.cpp
	source/allio/impl/implementation.cpp
	source/allio/impl/manual_multiplexer.cpp
	source/allio/impl/multiplexer.cpp
	source/allio/impl/platform_handle.cpp
	source/allio/impl/process_handle.cpp
	source/allio/impl/result.cpp
	source/allio/impl/socket_handle.cpp
	source/allio/impl/unique_name.cpp
)
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	FetchContent_Declare(
		win32
		GIT_REPOSITORY https://github.com/vasama/win32.git
		GIT_TAG master
	)
	FetchContent_MakeAvailable(win32)

	target_sources(allio
		PRIVATE
			source/allio/impl/posix_socket.cpp
			source/allio/impl/win32/default_multiplexer.cpp
			source/allio/impl/win32/directory_handle.cpp
			source/allio/impl/win32/encoding.cpp
			source/allio/impl/win32/file_handle.cpp
			source/allio/impl/win32/filesystem_handle.cpp
			source/allio/impl/win32/iocp_file_handle.cpp
			source/allio/impl/win32/iocp_multiplexer.cpp
			source/allio/impl/win32/iocp_process_handle.cpp
			source/allio/impl/win32/iocp_socket_handle.cpp
			source/allio/impl/win32/kernel.cpp
			source/allio/impl/win32/kernel_path.cpp
			source/allio/impl/win32/nt_error.cpp
			source/allio/impl/win32/platform_handle.cpp
			source/allio/impl/win32/posix_socket.cpp
			source/allio/impl/win32/process_handle.cpp
			source/allio/impl/win32/sync_byte_io.cpp
			source/allio/impl/win32/unique_handle.cpp
			source/allio/impl/win32/wsa_ex.cpp
	)
	target_link_libraries(allio
		PRIVATE
			win32
	)
endif()
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	target_sources(allio
		PRIVATE
			source/allio/impl/posix_socket.cpp
			source/allio/impl/linux/default_multiplexer.cpp
			source/allio/impl/linux/file_handle.cpp
			source/allio/impl/linux/filesystem_handle.cpp
			source/allio/impl/linux/io_uring_file_handle.cpp
			source/allio/impl/linux/io_uring_multiplexer.cpp
			source/allio/impl/linux/io_uring_socket_handle.cpp
			source/allio/impl/linux/platform_handle.cpp
			source/allio/impl/linux/posix_socket.cpp
			source/allio/impl/linux/process_handle.cpp
			source/allio/impl/linux/unique_fd.cpp
	)
endif()
target_include_directories(allio
	PUBLIC
		include
	PRIVATE
		source
)
target_compile_features(allio
	PUBLIC
		cxx_std_23
)
target_link_libraries(allio
	PUBLIC
		expected
		unifex
)
if(MSVC)
	target_compile_options(allio
		PUBLIC
			/Zc:preprocessor
	)
endif()


if(PROJECT_IS_TOP_LEVEL)
	add_executable(allio-test
		source/allio/impl/file_handle.test.cpp
		source/allio/impl/directory_handle.test.cpp
		source/allio/impl/path_view.test.cpp
		source/allio/impl/process_handle.test.cpp
		source/allio/impl/socket_handle.test.cpp
	)
	if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
		target_sources(allio-test
			PRIVATE
				source/allio/impl/win32/kernel_path.test.cpp
		)
		target_link_libraries(allio-test
			PRIVATE
				win32
		)
	endif()
	target_include_directories(allio-test
		PRIVATE
			source
	)
	target_link_libraries(allio-test
		PRIVATE
			allio
			Catch2::Catch2WithMain
	)
	add_dependencies(allio-test
		add_numbers
	)
	target_compile_definitions(allio-test
		PRIVATE
			# Pass the add_numbers binary path to the unit test as a macro.
			"allio_detail_ADD_NUMBERS_PATH=\"$<TARGET_FILE:add_numbers>\""
	)

	if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
		add_fuzz_test(allio-fuzz-kernel-path
			source/allio/impl/win32/kernel_path.fuzz.cpp
		)
		target_include_directories(allio-fuzz-kernel-path
			PRIVATE
				source
		)
		target_link_libraries(allio-fuzz-kernel-path
			PRIVATE
				allio
				win32
		)
	endif()

	add_subdirectory(tools)
endif()
