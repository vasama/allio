from conan import ConanFile

class Package(ConanFile):
	python_requires = "vsm.conan/0.1"
	python_requires_extend = "vsm.conan.base"
