cmake_minimum_required(VERSION 3.24)

set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
# Keep these commented line
# set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")

# Append our custom cmake modulespath to the path CMAKE_MODULE_PATH
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

# Place executables and libraries in  {build}/bin folder for convenience
# these 2 next lines needs to be set before project() and conan  calls
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


# Set the C++ standard to C++20 for all targets
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIB_NAME funcflow)

# Set the project name and version
project(${LIB_NAME} VERSION "${PROJECT_VERSION}" DESCRIPTION "A header-only C++20 library for building type-safe, functor-based data processing pipelines over ranges.")

# Cross-compilers
message(STATUS CMAKE_C_COMPILER ${CMAKE_C_COMPILER})
message(STATUS CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER})
message(STATUS CMAKE_LINKER ${CMAKE_LINKER})


# Define interface library
add_library(${LIB_NAME} INTERFACE)

file(GLOB HEADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/funcflow/*.hpp)

target_sources(${LIB_NAME} INTERFACE
    ${HEADER_FILES}
)

target_include_directories(${LIB_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/include
    $<INSTALL_INTERFACE:include>
)
target_compile_features(${LIB_NAME} INTERFACE cxx_std_20)


# OpenMP support (optional)
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found: ${OpenMP_CXX_VERSION}")
    target_link_libraries(${LIB_NAME} INTERFACE OpenMP::OpenMP_CXX)
    target_compile_definitions(${LIB_NAME} INTERFACE -DHAVE_OPENMP=1)
    # MSVC's default /openmp flag only supports basic OpenMP 2.0 , we need /openmp:llvm for newer versions
    # supporting OpenMP 3+ features
    target_compile_options(${LIB_NAME} INTERFACE $<$<CXX_COMPILER_ID:MSVC>:/openmp:llvm>)
else()
    message(STATUS "OpenMP not found - parallel execution will run in sequential mode")
    target_compile_definitions(${LIB_NAME} INTERFACE -DHAVE_OPENMP=0)
endif()


enable_testing()
add_subdirectory (tests)