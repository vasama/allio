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


add_library(allio
	source/directory_handle.cpp
	source/default_multiplexer.cpp
	source/file_handle.cpp
	source/filesystem_handle.cpp
	source/handle.cpp
	source/manual_multiplexer.cpp
	source/multiplexer.cpp
	source/platform_handle.cpp
	source/result.cpp
	source/socket_handle.cpp
	source/static_multiplexer_handle_relation_provider.cpp
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
			source/posix_socket.cpp
			source/win32/api_string.cpp
			source/win32/default_multiplexer.cpp
			source/win32/file_handle.cpp
			source/win32/filesystem_handle.cpp
			source/win32/iocp_file_handle.cpp
			source/win32/iocp_multiplexer.cpp
			source/win32/iocp_socket_handle.cpp
			source/win32/kernel.cpp
			source/win32/kernel_error.cpp
			source/win32/platform_handle.cpp
			source/win32/posix_socket.cpp
			source/win32/wsa_error.cpp
			source/win32/wsa_ex.cpp
	)
	target_link_libraries(allio
		PRIVATE
			win32
	)
endif()
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	target_sources(allio
		PRIVATE
			source/posix_socket.cpp
			source/linux/default_multiplexer.cpp
			source/linux/file_handle.cpp
			source/linux/filesystem_handle.cpp
			source/linux/io_uring_file_handle.cpp
			source/linux/io_uring_multiplexer.cpp
			source/linux/io_uring_socket_handle.cpp
			source/linux/platform_handle.cpp
			source/linux/posix_socket.cpp
			source/linux/unique_fd.cpp
	)
endif()
target_include_directories(allio
	PUBLIC
		include
)
target_compile_features(allio
	PUBLIC
		cxx_std_20
)
target_link_libraries(allio
	PUBLIC
		expected
		unifex
)
if(MSVC)
	target_compile_options(allio
		PRIVATE
			/Zc:preprocessor
	)
endif()


if(PROJECT_IS_TOP_LEVEL)
	add_executable(allio-test
		source/file_handle.test.cpp
		source/path_view.test.cpp
		source/socket_handle.test.cpp
	)
	target_link_libraries(allio-test
		PRIVATE
			allio
			Catch2::Catch2WithMain
	)
endif()
