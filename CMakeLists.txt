cmake_minimum_required(VERSION 3.24)
project(allio)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

find_package(vsm.cmake REQUIRED)
vsm_add_root(allio)

#if(${CMAKE_CXX_COMPILER_ID} STREQUAL Clang)
#	add_compile_options("-fprofile-instr-generate" "-fcoverage-mapping")
#endif()

add_subdirectory(library)
add_subdirectory(testing)
