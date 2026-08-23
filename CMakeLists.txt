cmake_minimum_required(VERSION 3.16)

project(tiny_delegate VERSION 1.0.0 LANGUAGES CXX)

# Header-only library target: tiny_delegate::tiny_delegate.
add_library(tiny_delegate INTERFACE)
add_library(tiny_delegate::tiny_delegate ALIAS tiny_delegate)
target_include_directories(tiny_delegate INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(tiny_delegate INTERFACE cxx_std_17)

# Host tests build only when this project is configured directly, never for
# consumers via add_subdirectory()/FetchContent. The Qt demo app (delegate.pro)
# is built separately with qmake and is not part of this build.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(TINY_DELEGATE_BUILD_TESTS "Build the tiny_delegate test suite" ON)
else()
    option(TINY_DELEGATE_BUILD_TESTS "Build the tiny_delegate test suite" OFF)
endif()

if(TINY_DELEGATE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
