add_library(
	example-stable-abi-library SHARED
	library.cpp
	library.hpp
)
target_link_libraries(
	example-stable-abi-library
	PRIVATE
		allio
)

add_executable(
	example-stable-abi-application
	application.cpp
)
target_link_libraries(
	example-stable-abi-application
	PRIVATE
		allio
		example-stable-abi-library
)
