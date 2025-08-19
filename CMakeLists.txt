cmake_minimum_required(VERSION 3.16)

set(vxsdr_examples_version_major  1)
set(vxsdr_examples_version_minor  0)
set(vxsdr_examples_version_patch  0)

set(vxsdr_examples_version ${vxsdr_examples_version_major}.${vxsdr_examples_version_minor}.${vxsdr_examples_version_patch})

project(vxsdr_examples VERSION ${vxsdr_examples_version} DESCRIPTION "VXSDR examples in C++" LANGUAGES CXX)

option(DO_STATIC_ANALYSIS "Run clang-tidy during builds" OFF)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type defined -- assuming Release")
    message(STATUS "Use cmake -DCMAKE_BUILD_TYPE=<type> to specify build type")
    set(CMAKE_BUILD_TYPE Release)
else()
    message(STATUS "Build type is ${CMAKE_BUILD_TYPE}")
endif()

if(DO_STATIC_ANALYSIS)
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(vxsdr_examples_build_defs -DVERSION_STRING="${vxsdr_examples_version}"
                              -DVERSION_MAJOR=${vxsdr_examples_version_major}
                              -DVERSION_MINOR=${vxsdr_examples_version_minor}
                              -DVERSION_PATCH=${vxsdr_examples_version_patch}
                              -DCOMPILER_INFO="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
                              -DSYSTEM_INFO="${CMAKE_SYSTEM}")

find_package(vxsdr REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(vxsdr_tx_loop_file_source source/vxsdr_tx_loop_file.cpp
                              source/host_radio_options.cpp
                              source/utility.cpp)

add_executable(vxsdr_tx_loop_file ${vxsdr_tx_loop_file_source})
target_compile_definitions(vxsdr_tx_loop_file PRIVATE ${vxsdr_examples_build_defs})
target_compile_features(vxsdr_tx_loop_file PUBLIC cxx_std_20)
set_target_properties(vxsdr_tx_loop_file PROPERTIES CXX_STANDARD_REQUIRED ON
                                                    CXX_EXTENSIONS OFF)
target_include_directories(vxsdr_tx_loop_file PRIVATE include)
target_link_libraries(vxsdr_tx_loop_file PRIVATE vxsdr::libvxsdr Threads::Threads)
