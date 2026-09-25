from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class FitViewerRecipe(ConanFile):
    """Consumer recipe: declares dependencies and generates the CMake glue.

    Typical use (see README):
        conan install . --build=missing
        cmake --preset conan-release
        cmake --build --preset conan-release
    """

    name = "fitviewer"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"
    options = {"with_gui": [True, False]}
    default_options = {"with_gui": True}

    def requirements(self):
        # test_requires: only needed to build/run our tests, never propagated
        # to anyone consuming this package.
        self.test_requires("gtest/1.17.0")
        # Qt is taken from the system (apt/brew/installer) on purpose: building
        # it through Conan works but compiles Qt from source (~1h+).

    def layout(self):
        # Puts build output in build/<BuildType>/ and the generated toolchain
        # and find-modules in build/<BuildType>/generators/.
        cmake_layout(self)

    def generate(self):
        CMakeDeps(self).generate()  # -> GTestConfig.cmake etc.
        tc = CMakeToolchain(self)  # -> conan_toolchain.cmake + CMakePresets.json
        tc.cache_variables["FITVIEWER_BUILD_GUI"] = bool(self.options.with_gui)
        tc.generate()

    def build(self):
        # Used by `conan build .` (install + configure + build + test in one go).
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if not self.conf.get("tools.build:skip_test", default=False):
            cmake.test()
