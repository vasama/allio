function(allio_add_error_encoding name target sources_target)
	set(sources_file "${CMAKE_CURRENT_BINARY_DIR}/allio-ec-${name}-sources.txt")
	set(script_file "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-error-encoding.py")
	set(include_dir "${CMAKE_CURRENT_BINARY_DIR}/${name}-ec-include")
	set(inline_file "${include_dir}/allio_error_encoding.hpp")
	set(encoding "allio_error_encoding_${name}")

	get_property(
		sources_aliased_target
		TARGET "${sources_target}"
		PROPERTY ALIASED_TARGET
	)

	if(DEFINED sources_aliased_target)
		set(sources_target "${sources_aliased_target}")
	endif()

	get_property(
		sources
		TARGET "${sources_target}"
		PROPERTY SOURCES
	)

	set(sources_file_content "${sources}")
	list(TRANSFORM sources_file_content APPEND "\n")
	file(WRITE "${sources_file}" ${sources_file_content})

	add_custom_command(
		OUTPUT "${inline_file}"
		COMMAND
			python3 "\"${script_file}\""
			--encoding "${encoding}"
			--sources "\"${sources_file}\""
			--outfile "\"${inline_file}\""
		DEPENDS "${script_file}" ${sources}
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	)

	set(custom_target "allio-ec-generate-${name}")
	add_custom_target("${custom_target}" DEPENDS "${inline_file}")
	set_target_properties("${custom_target}" PROPERTIES FOLDER "CustomTargets")

	set(interface_target "allio-ec-target-${name}")
	add_library("allio-ec-target-${name}" INTERFACE)

	add_dependencies("${interface_target}" "${custom_target}")
	target_compile_definitions("${interface_target}" INTERFACE "allio_error_encoding=${encoding}")
	target_include_directories("${interface_target}" INTERFACE "${include_dir}")

	add_library("${target}" ALIAS "${interface_target}")
endfunction()
