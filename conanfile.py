from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class packageRecipe(ConanFile):
    name = "tm4c_i2c"
    version = "1.0.0"

    license = "Software only for internal use @ noEmbedded"
    author = "Adrian Werel, adrian.werel@noembedded.com"
    description = "Tm4c I2C."
    topics = ("tm4c", "i2c")

    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [False]}
    default_options = {"shared": False}
    generators = "CMakeToolchain", "CMakeDeps"
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "test/*"

    def validate(self):
        check_min_cppstd(self, 14)

    def requirements(self):
        self.requires("error/[~1]")
        self.requires("i_callback/[~1]", transitive_headers=True)
        self.requires("i_i2c/[~1]", transitive_headers=True)
        self.requires("i_irq_handler/[~1]")
        self.requires("i_processable/[~1]")
        self.requires("i_serial_port/[~1]")
        self.requires("i_sysclk/[~1]")
        self.requires("tm4c_system/[~1]")
        self.requires("locator/[~1]")
            
        if not self.conf.get("tools.build:skip_test", default=False):
            self.test_requires("gtest/1.16.0")

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure(variables={"PACKAGE_NAME": self.name})
        cmake.build()
        if not self.conf.get("tools.build:skip_test", default=False):
            cmake.ctest(cli_args=["--test-dir test"])

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = [self.name]
