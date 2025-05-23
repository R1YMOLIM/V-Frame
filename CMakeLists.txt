
cmake_minimum_required(VERSION 3.15)
project(VFrame CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "C:/GLFW")

# Find packages
find_package(glfw3 REQUIRED)

add_library(vframe SHARED
    src/window.c
    src/window.cpp
)

target_include_directories(vframe PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link them
target_link_libraries(vframe PRIVATE glfw)
