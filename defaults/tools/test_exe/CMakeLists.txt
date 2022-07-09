vsm_add_executable(
	test_exe
	FOLDER Tools

	PROPERTIES
		allio_output "${CMAKE_CURRENT_BINARY_DIR}/test_exe_output.txt"

	SOURCES
		test_exe.cpp

	SOURCE_DEFINITIONS
		"allio_detail_test_exe_output=\"$<TARGET_PROPERTY:test_exe,allio_output>\""

	SOURCE_DEPENDENCIES
		vsm::core
)

#add_executable(test_exe "test_exe.cpp")
#
#set_target_properties(
#	test_exe
#
#	PROPERTIES
#		FOLDER "Tools"
#		allio_output "${CMAKE_CURRENT_BINARY_DIR}/test_exe_output.txt"
#)
#
#target_compile_definitions(
#	test_exe
#
#	PRIVATE
#		"allio_detail_test_exe_output=\"$<TARGET_PROPERTY:test_exe,allio_output>\""
#)
