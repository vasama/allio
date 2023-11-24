cmake_minimum_required(VERSION 3.23)

project(allio-root)

include(vsm)
vsm_add_aggregate_test(allio_test)

add_subdirectory(allio)
add_subdirectory(tls)
