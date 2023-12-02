add_executable(test_program "test_program.cpp")

target_compile_definitions(test_program
	PRIVATE
		"allio_detail_test_program_output=\"${allio_test_program_output}\""
)
