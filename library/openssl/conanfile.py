from conan import ConanFile

class package(ConanFile):
	python_requires = "vsm_conan/0.1"
	python_requires_extend = "vsm_conan.base"

	vsm_name = "allio-openssl"
	version = "0.1"

	requires = (
		"allio/0.1",
		"openssl/[^3.14]"
	)
