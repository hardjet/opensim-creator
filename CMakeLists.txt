cmake_minimum_required(VERSION 3.15)

project(osc VERSION 0.5.11 LANGUAGES CXX)


# -------------- gather user-facing build cache vars ---------------- #

set(
    OSC_USE_CLANG_TIDY OFF
    CACHE BOOL
    "enable/disable running `clang-tidy` (linter) while building"
)
set(
    OSC_USE_IWYU OFF
    CACHE BOOL
    "enable/disable running `iwyu` (include-what-you-use) while building"
)
set(
    OSC_IWYU_PATH "include-what-you-use"
    CACHE STRING
    "filepath to the `iwyu` executable (if not on the PATH)"
)
set(
    OSC_FORCE_ASSERTS_ENABLED ON
    CACHE BOOL
    "force-enable/-disable OSC's runtime assertions - even if building a release build"
)
set(
    OSC_FORCE_UNDEFINE_NDEBUG OFF
    CACHE BOOL
    "force-enable/disable NDEBUG - even if building a release build. Note: unlike OSC_FORCE_ASSERTS_ENABLED, this also tends to enable runtime assertions from library headers"
)
set(
    OSC_USE_ASAN OFF
    CACHE BOOL
    "enable/disable address sanitizer (libASAN): useful for spotting memory access violations and leaks"
)
set(
    OSC_DEFAULT_RESOURCE_DIR "../resources"
    CACHE STRING
    "the default filesystem location of installed runtime resources dir relative to the application executable when it is not specified in the user's configuration file"
)
set(
    OSC_REPO_URL "https://github.com/ComputationalBiomechanicsLab/opensim-creator"
    CACHE STRING
    "the internet location of the software's repo - used by in-app links, installers, etc."
)
set(
    OSC_ORGNAME "cbl"
    CACHE STRING
    "the name of the organization that created the application (affects configuration location)"
)
set(
    OSC_APPNAME "osc"
    CACHE STRING
    "the short name for the application (affects configuration location)"
)
set(
    OSC_LONG_APPNAME "OpenSim Creator"
    CACHE STRING
    "a longer, user-friendly name of the application"
)
set(
    OSC_PACKAGE_NAME "OpenSimCreator"
    CACHE STRING
    "a prefix for the name of the built package (e.g. OSC_PACKAGE_NAME-version.exe)"
)
set(
    OSC_SOFTWARE_DESCRIPTION "A standalone GUI for building OpenSim models"
    CACHE STRING
    "a desription of what the software does (shows in package installers on Debian, for example)"
)
set(
    OSC_AUTHOR "Adam Kewley"
    CACHE STRING
    "the user-facing name of the author (appears in installer, etc.)"
)
set(
    OSC_AUTHOR_EMAIL "contact@adamkewley.com"
    CACHE STRING
    "the user-facing email of the author (appears in 'email support', installer, etc.)"
)
set(OSC_BUILD_OPENSIMCREATOR ON
    CACHE BOOL
    "enable/disable building OpenSim Creator (i.e. the OpenSim-dependent part of the codebase)"
)
set(
    OSC_BUILD_DOCS OFF
    CACHE BOOL
    "enable/disable building the documentation (requires that sphinx-build is available on the PATH)"
)
set(
    OSC_BUILD_BENCHMARKS ON
    CACHE BOOL
    "enable/disable building the benchmark suite (handy for development)"
)
set(OSC_EXECUTABLE_SUFFIX ${CMAKE_EXECUTABLE_SUFFIX}
    CACHE BOOL
    "global suffix to place on the end of each executable (platform-dependent)"
)
set(OSC_BUILD_INSTALLABLE_PACKAGE ON
    CACHE BOOL
    "enable/disable building installable packages (exe self-installer, .deb package, etc.)"
)
set(OSC_DISCOVER_TESTS ON
    CACHE BOOL
    "enable/disable automatically running test discovery (IDE integration)"
)
set(
    OSC_EMSCRIPTEN OFF
    CACHE BOOL
    "enable special build parameters for emscripten (emsdk) - don't try this unless you know what you're doing ;)"
)

# set OSC_BUILD_ID
#
# this is so that errors in the binary can potentially be linked to the source
# that produced the binary later on (user errors, etc.)
if(TRUE)

    # compute OSC_DEFAULT_BUILD_ID
    if(DEFINED ENV{GITHUB_RUN_ID})
        # build is running inside GitHub Action CI - use that ID
        set(OSC_DEFAULT_BUILD_ID "GITHUB_$ENV{GITHUB_RUN_ID}.$ENV{GITHUB_RUN_NUMBER}")
    else()
        # build is probably running on a dev's machine - generate a random ID
        string(RANDOM RANDOM_ID)
        set(OSC_DEFAULT_BUILD_ID "CUSTOM_${RANDOM_ID}")
        unset(RANDOM_ID)
    endif()

    set(
        OSC_BUILD_ID "${OSC_DEFAULT_BUILD_ID}"
        CACHE STRING
        "a unique identifier for this particular build configuration (handy for bug tracing)"
    )
    unset(OSC_DEFAULT_BUILD_ID)
endif()


# -------------- set/mutate internal vars ---------------- #

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_EXECUTABLE_SUFFIX ${OSC_EXECUTABLE_SUFFIX})

# set LINUX
#
# this is based on whether the UNIX variant being used appears to be LINUX
if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

# set CMAKE_CXX_INCLUDE_WHAT_YOU_USE
#
# this enables CMake's in-built support for iwyu
if(OSC_USE_IWYU)
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${OSC_IWYU_PATH};-Xiwyu;any;-Xiwyu;iwyu;-Xiwyu;")
endif()

# set CMAKE_CXX_CLANG_TIDY
#
# this enables CMake's in-built support for clang-tidy
if(OSC_USE_CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy")
endif()

# handle OSC_FORCE_UNDEFINE_NDEBUG
#
# this unsets the relevant flags globally
if(OSC_FORCE_UNDEFINE_NDEBUG)
    # MSVC
    string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    # others
    string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
endif()


# -------------- setup top-level project state ---------------- #

# enable testing in this top-level project, so that `ctest` works when
# (e.g.) IDEs get a list of tests when they use `ctest --show-only`
# to list all tests project-wide
#
# see:
#
# - https://cmake.org/cmake/help/latest/command/enable_testing.html
# - https://developercommunity.visualstudio.com/t/test-explorer-doesnt-detect-tests/482451
# - https://stackoverflow.com/a/67984312
enable_testing()

# -------------- add/include subdirectories for each part of the project ---------------- #

add_subdirectory(src)
add_subdirectory(apps)
add_subdirectory(tests)

if(${OSC_BUILD_DOCS})
    add_subdirectory(docs)
endif()

if(${OSC_BUILD_BENCHMARKS})
    add_subdirectory(benches)
endif()

if (${OSC_BUILD_INSTALLABLE_PACKAGE})
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Packaging.cmake)
endif()
