from conan import ConanFile

class package(ConanFile):
	python_requires = "vsm_tools/0.1"
	python_requires_extend = "vsm_tools.base"

	vsm_name = "allio"
	version = "0.1"

	requires = (
		"allio_core/0.1",
		"vsm_intrusive/0.1",
		"p2300/0.1@vasama",
	)

	vsm_libs = ["allio_defaults"]
