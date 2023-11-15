from conan import ConanFile

class package(ConanFile):
	python_requires = "vsm_tools/0.1"
	python_requires_extend = "vsm_tools.base"

	vsm_name = "allio"
	version = "0.1"

	requires = (
		#TODO: Why doesn't semver requirement work here?!
		"vsm_box/0.1",
		"vsm_core/0.1",
		"vsm_flags/0.1",
		"vsm_intrusive/0.1",
		"vsm_intrusive_ptr/0.1",
		"vsm_linear/0.1",
		"vsm_literals/0.1",
		"vsm_math/0.1",
		"vsm_offset_ptr/0.1",
		"vsm_optional/0.1",
		"vsm_result/0.1",
		"vsm_tag_ptr/0.1",
		"vsm_unique_resource/0.1",
		"p2300/ea081cdd1e65d31b4be954addc99a61608f8d7c3"
	)
