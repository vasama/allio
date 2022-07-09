add_executable(file_test
	"file.cpp"
)
target_link_libraries(file_test
	PRIVATE
		allio
)

add_executable(path_cli
	"path_cli.cpp"
)
target_link_libraries(path_cli
	PRIVATE
		allio
)
