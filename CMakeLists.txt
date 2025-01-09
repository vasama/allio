cmake_minimum_required(VERSION 3.24)
project(allio)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

find_package(vsm.cmake REQUIRED)
vsm_add_root(allio)

add_subdirectory(library)
