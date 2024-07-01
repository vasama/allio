cmake_minimum_required(VERSION 3.24)

project(allio)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

find_package(vsm-cmake REQUIRED)
include(vsm)
vsm_add_aggregate_test(allio_test)

add_subdirectory(core)
add_subdirectory(defaults)
#add_subdirectory(tls)

#add_subdirectory(dns)
#add_subdirectory(rpc)
#add_subdirectory(usb)
