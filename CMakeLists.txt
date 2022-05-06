cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

# To use `vcpkg`, point to its toolchain file during CMake configuration,
# for instance with `-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`,
# assuming that `VCPKG_ROOT` points to `vcpkg`'s directory

# In case `vcpkg` is used
list(APPEND VCPKG_FEATURE_FLAGS "versions")

# Sets the `variable` to `value` if the `variable` is not already defined
function(set_if_not_defined variable value)
    if(NOT DEFINED ${variable})
        set(${variable} "${value}" PARENT_SCOPE)
    endif()
endfunction()

# For CMake 3.20, this isn't needed for 3.21+
if(DEFINED ENV{CMAKE_TOOLCHAIN_FILE})
    set_if_not_defined(CMAKE_TOOLCHAIN_FILE "$ENV{CMAKE_TOOLCHAIN_FILE}")
endif()

project(hareflow LANGUAGES CXX)
enable_testing()
set_if_not_defined(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE "PRE_TEST") # TODO: Remove once qpid-proton RPATH is fixed

option(BUILD_TESTS "Build tests" OFF)
option(BUILD_INTEGRATION "Build integration tests" OFF)
option(BUILD_WARNINGS "Build with hareflow's warnings setting" ON)

include(GNUInstallDirs)
set_if_not_defined(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")
set_if_not_defined(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")
set_if_not_defined(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
set(INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}" CACHE STRING "Installation directory for libraries")
set(INSTALL_BINDIR "${CMAKE_INSTALL_BINDIR}" CACHE STRING "Installation directory for dll")
set(INSTALL_EXEDIR "${CMAKE_INSTALL_BINDIR}" CACHE STRING "Installation directory for executables")
set(INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}"
    CACHE STRING "Installation directory for header files")
set(INSTALL_SHAREDIR "share/${PROJECT_NAME}" CACHE STRING "Installation directory for shared files")
set(INSTALL_CMAKEDIR "share/${PROJECT_NAME}" CACHE STRING "Installation directory for CMake files")

if(BUILD_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU") # ref https://gcc.gnu.org/onlinedocs/gcc/Invoking-GCC.html
        add_compile_options(
            -Wall
            -Werror
            -Wextra
        )
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC") # ref https://docs.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-alphabetically?view=msvc-160
        add_compile_options(
            /W4
            /WX
        )
    endif()
endif()


add_subdirectory(src)

if(BUILD_INTEGRATION OR BUILD_TESTS)
    add_subdirectory(googletest)
endif()
if(BUILD_INTEGRATION)
    add_subdirectory(integration)
endif()
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()


# The front slash is important: we want to install the _content_ of the directory
install(DIRECTORY include/
    COMPONENT dev
    DESTINATION ${INSTALL_INCLUDEDIR}
)
install(EXPORT ${PROJECT_NAME}-targets
    COMPONENT dev

    DESTINATION "${INSTALL_CMAKEDIR}"
    NAMESPACE ${PROJECT_NAME}::
)

include(CMakePackageConfigHelpers)
configure_package_config_file(${PROJECT_NAME}-config.cmake.in
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
    INSTALL_DESTINATION "${INSTALL_CMAKEDIR}"
)
install(FILES ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
    COMPONENT dev
    DESTINATION ${INSTALL_CMAKEDIR}
)


include("${CMAKE_CURRENT_LIST_DIR}/CMakeCPack.cmake")
