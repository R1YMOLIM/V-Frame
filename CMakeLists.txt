
cmake_minimum_required(VERSION 3.15)
project(VFrame C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(INCLUDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Find packages yea
find_package(Vulkan REQUIRED)

add_library(vFrame SHARED
    src/window.cpp
    src/vf_vulkan.cpp
 "src/user_realisation/vf_application.cpp")

if (MSVC)
    message(STATUS "Configuring for MSVC: using dynamic CRT (/MD)")
    set(GLFW_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/glfw-3.4/lib-vc2022")
    include_directories(${INCLUDE_ROOT}/glfw-3.4)
    target_link_libraries(vFrame PRIVATE ${GLFW_LIB_DIR}/glfw3.lib)
    # Add warnings
    add_compile_options(/W4 /permissive-)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Configuring for Clang or GCC")
    find_package(glfw3 REQUIRED)
    target_link_libraries(vFrame PRIVATE glfw)  # Замінив Minecraft_Vulkan на vFrame, щоб було коректно
    # Add warnings
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
else()
    message(WARNING "Unknown compiler: manual configuration may be needed")
endif()

target_include_directories(vFrame PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Додаєм щоб вона не поводилась що ми її імпортуєм а щоб повидиласб як експортуєм
target_compile_definitions(vFrame PRIVATE VFRAME_BUILD_DLL)

target_include_directories(vFrame PRIVATE ${Vulkan_INCLUDE_DIRS})

# Link them
target_link_libraries(vFrame PRIVATE ${Vulkan_LIBRARIES})
