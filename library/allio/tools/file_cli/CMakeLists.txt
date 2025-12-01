vsm_add_executable(
	file_cli

	INTERNAL
	FOLDER Tools

	SOURCES
		file_cli.cpp

	SOURCE_DEPENDENCIES
		allio::blocking
)
