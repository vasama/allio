function(allio_add_error_encoding target)
	get_property(
		aliased_target
		TARGET "${target}"
		PROPERTY ALIASED_TARGET
	)

	if(DEFINED aliased_target)
		set(target "${aliased_target}")
	endif()

	get_property(
		sources
		TARGET "${target}"
		PROPERTY SOURCES
	)

	set(script_file "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-error-encoding.py")
	set(include_dir "${CMAKE_CURRENT_BINARY_DIR}/include")
	set(inline_file "${include_dir}/allio_error_encoding.hpp")
	set(encoding "allio_error_encoding_${target}")

	add_custom_command(
		OUTPUT "${inline_file}"
		COMMAND
			python3 "${script_file}"
			--encoding "${encoding}"
			--sources "\"${sources}\""
			--outfile "${inline_file}"
		DEPENDS "${script_file}" ${sources}
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	)

	add_custom_target(
		"${target}-error-encoding"
		DEPENDS "${inline_file}"
	)

	add_dependencies("${target}" "${target}-error-encoding")
	target_compile_definitions("${target}" PRIVATE "allio_error_encoding=${encoding}")
	target_include_directories("${target}" PRIVATE "${include_dir}")
endfunction()
