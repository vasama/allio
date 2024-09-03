from conan import ConanFile

class package(ConanFile):
	python_requires = "vsm_conan/0.1"
	python_requires_extend = "vsm_conan.base"

	vsm_name = "allio::core"
	version = "0.1"

	requires = (
		"vsm_core/0.1",
		"vsm_intrusive/0.1",
		"vsm_intrusive_ptr/0.1",
		"vsm_linear/0.1",
		"vsm_literals/0.1",
		"vsm_math/0.1",
		"vsm_numeric/0.1",
		"vsm_offset_ptr/0.1",
		"vsm_result/0.1",
		"vsm_tag_ptr/0.1",
		"vsm_unique_resource/0.1",
	)

	vsm_libs = ["allio_core"]
