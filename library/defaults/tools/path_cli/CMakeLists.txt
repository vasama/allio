add_executable(path_cli
	"path_cli.cpp"
)
set_target_properties(
	path_cli

	PROPERTIES
		FOLDER "Tools"
)
target_include_directories(path_cli
	PRIVATE
		# Requires <allio/test/reverse_iterator.hpp>
		../../source
)
target_link_libraries(path_cli
	PRIVATE
		allio::core
)
