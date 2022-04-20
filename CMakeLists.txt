cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

option(BUILD_TESTS "Build tests" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(USE_VCPKG "Use vcpkg for dependencies" OFF)

if (USE_VCPKG)
    set(VCPKG_FEATURE_FLAGS "versions")

    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE $ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
            CACHE STRING "Vcpkg toolchain file")
    else()
        set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake
            CACHE STRING "Vcpkg toolchain file")
    endif()

    if (BUILD_TESTS)
        list(APPEND VCPKG_MANIFEST_FEATURES "tests")
    endif()

    if (BUILD_BENCHMARKS)
        list(APPEND VCPKG_MANIFEST_FEATURES "benchmarks")
    endif()
endif()

if (BUILD_TESTS OR BUILD_BENCHMARKS)
    enable_testing()
endif()

project(hareflow)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(fmt REQUIRED)
find_package(ProtonCpp REQUIRED)

add_library(hareflow)

if ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
  add_compile_options("-Wall" "-Wextra")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  add_compile_options("/W4" "/wd4251")
endif()

target_compile_definitions(hareflow
    PRIVATE
        HAREFLOW_EXPORTS
)
if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_compile_definitions(hareflow
        PRIVATE
            _WIN32_WINNT=0x603
    )
endif()

target_link_libraries(hareflow
    PUBLIC
        Boost::boost
        fmt::fmt
        
    PRIVATE
        Proton::cpp
        OpenSSL::SSL
)

include(GNUInstallDirs)
target_include_directories(hareflow
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set(INSTALL_CMAKEDIR "${CMAKE_INSTALL_DATAROOTDIR}/hareflow")
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS hareflow EXPORT hareflow-targets)
install(EXPORT hareflow-targets
    DESTINATION "${INSTALL_CMAKEDIR}"
    NAMESPACE hareflow::
)
include(CMakePackageConfigHelpers)
configure_package_config_file(hareflow-config.cmake.in
    ${PROJECT_BINARY_DIR}/hareflow-config.cmake
    INSTALL_DESTINATION "${INSTALL_CMAKEDIR}"
)
install(FILES ${PROJECT_BINARY_DIR}/hareflow-config.cmake DESTINATION ${INSTALL_CMAKEDIR})

if (BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if (BUILD_TESTS)
    add_subdirectory(tests)
endif()

target_sources(hareflow
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow.h
        
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/client.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/client_parameters.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/codec.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/consumer.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/consumer_builder.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/environment.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/environment_builder.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/exceptions.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/io_context_holder.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/logging.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/message_builder.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/producer.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/producer_builder.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/qpid_proton_codec.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/stream_creator.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/types.h

        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/detail/accumulator.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/detail/background_scheduler.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/detail/binary_buffer.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/detail/binary_buffer.hpp
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/detail/client_impl.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/detail/client_impl.hpp
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/detail/commands.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/detail/connection.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/detail/consumer_impl.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/detail/environment_impl.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/detail/internal_types.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/detail/producer_impl.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/detail/reschedulable_task.h
        ${CMAKE_CURRENT_LIST_DIR}/include/hareflow/detail/semaphore.h
        
        ${CMAKE_CURRENT_LIST_DIR}/src/accumulator.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/background_scheduler.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/binary_buffer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/client_impl.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/client_parameters.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/codec.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/commands.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/connection.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/consumer_builder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/consumer_impl.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/environment_builder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/environment_impl.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/exceptions.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/io_context_holder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/logging.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/message_builder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/producer_builder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/producer_impl.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/qpid_proton_codec.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/reschedulable_task.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/stream_creator.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/types.cpp
)