import os
import re

from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.build import can_run

class testPackageRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def build(self):
        tested_reference_str_self = re.search("^(.+)/.*$", self.tested_reference_str)
        tested_reference_str_name = tested_reference_str_self.group(1)

        cmake = CMake(self)
        cmake.configure(variables={"PACKAGE_NAME" : tested_reference_str_name})
        cmake.build()

    def layout(self):
        cmake_layout(self)

    def test(self):
        if can_run(self):
            cmd = os.path.join(self.cpp.build.bindir, "test_package")
            self.run(cmd, env="conanrun")