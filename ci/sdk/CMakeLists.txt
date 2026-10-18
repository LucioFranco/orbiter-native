# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.19)
project(OrbiterSDKFoundation LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(portable_sdk_test ../../orbiter/Tests/PortableSDK.cpp)
target_include_directories(portable_sdk_test PRIVATE ../../orbiter/Orbitersdk/include)
enable_testing()
add_test(NAME portable_sdk COMMAND portable_sdk_test)
