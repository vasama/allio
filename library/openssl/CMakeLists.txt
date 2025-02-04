cmake_minimum_required(VERSION 3.23)
project(allio-openssl)

find_package(vsm.cmake REQUIRED)
vsm_define_package(allio.openssl)

vsm_add_library(
	allio::openssl

	HEADERS
		include/allio/openssl/socket.hpp

		include/allio/openssl/detail/datagram_socket.hpp
		include/allio/openssl/detail/listen_socket.hpp
		include/allio/openssl/detail/openssl.hpp
		include/allio/openssl/detail/socket.hpp

	SOURCES
		source/allio/impl/openssl/openssl.cpp

	HEADER_DEPENDENCIES
		vsm::intrusive
		allio::core

	SOURCE_DEPENDENCIES
		OpenSSL::SSL

	TEST_SOURCES
		source/allio/test/openssl.cpp
		source/allio/test/socket.cpp

	TEST_DEPENDENCIES
		allio::blocking
)

#TODO: Handle allio.openssl include paths a little smarter:
target_include_directories(allio_openssl PRIVATE ../core/source)


# Generate private keys and certificates for testing purposes:

set(test_secrets_dir "${CMAKE_CURRENT_BINARY_DIR}/allio-test-secrets")
set(test_secret_list "")

file(MAKE_DIRECTORY "${test_secrets_dir}")

set(client_private_key_file "${test_secrets_dir}/client-private-key.pem")
set(client_certificate_file "${test_secrets_dir}/client-certificate.pem")

set(server_private_key_file "${test_secrets_dir}/server-private-key.pem")
set(server_certificate_file "${test_secrets_dir}/server-certificate.pem")

function(allio_add_test_private_key name)
	set(private_key_file "${test_secrets_dir}/${name}-private-key.pem")

	add_custom_command(
		OUTPUT "${private_key_file}"
		COMMAND
			#TODO: Debugging
			C:/Users/vasama/.conan2/p/b/opensc2a66914737f6/p/bin/openssl genpkey
			-algorithm ed25519
			-out "${private_key_file}"
	)

	list(APPEND test_secret_list "${private_key_file}")
	set(test_secret_list "${test_secret_list}" PARENT_SCOPE)
endfunction()

function(allio_add_test_certificate name subject)
	set(private_key_file "${test_secrets_dir}/${name}-private-key.pem")
	set(certificate_file "${test_secrets_dir}/${name}-certificate.pem")

	add_custom_command(
		OUTPUT "${certificate_file}"
		DEPENDS "${private_key_file}"
		COMMAND
			#TODO: Debugging
			C:/Users/vasama/.conan2/p/b/opensc2a66914737f6/p/bin/openssl req
			-new
			-x509
			-sha256
			-days 365 #TODO: If possible, invalidate target based on date.
			-key "${private_key_file}"
			-out "${certificate_file}"
	)

	list(APPEND test_secret_list "${certificate_file}")
	set(test_secret_list "${test_secret_list}" PARENT_SCOPE)
endfunction()

allio_add_test_private_key("client")
allio_add_test_certificate("client" "/CN=example.com")

allio_add_test_private_key("server")
allio_add_test_certificate("server" "/CN=example.com")

add_custom_target(
	allio-generate-test-secrets
	DEPENDS ${test_secret_list}
)

add_library(allio-test-secrets INTERFACE)
add_dependencies(allio-test-secrets allio-generate-test-secrets)

target_compile_definitions(
	allio-test-secrets INTERFACE
	"allio_test_secret_path=\"${test_secrets_dir}\"")

vsm_configure(allio::openssl TEST_DEPENDENCIES allio-test-secrets)
