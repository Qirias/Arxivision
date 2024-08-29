cmake_minimum_required(VERSION 3.20)
project(ArXivision)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Suppress warnings for GCC/Clang
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")

# Define source files
file(GLOB_RECURSE SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/imgui/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/imgui/backends/imgui_impl_vulkan.cpp
)

# Add main.cpp
list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Find GLFW
find_library(GLFW_LIBRARY NAMES glfw glfw3 PATHS ${CMAKE_CURRENT_SOURCE_DIR}/bin/glfw/osx ${CMAKE_CURRENT_SOURCE_DIR}/libs/glfw)
if(NOT GLFW_LIBRARY)
    message(FATAL_ERROR "GLFW library not found. Please ensure it's installed in the project's libs/glfw directory.")
endif()

# Find GLFW include directory
find_path(GLFW_INCLUDE_DIR NAMES GLFW/glfw3.h PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/bin/glfw/osx/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/glfw/include
)
if(NOT GLFW_INCLUDE_DIR)
    message(FATAL_ERROR "GLFW include directory not found. Please ensure it's in the correct location.")
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/libs
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/imgui
    ${GLFW_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/vulkan
)

# Platform-specific configurations
if(APPLE)
    # macOS specific settings
    set(MACOS_ARCH "x86_64;arm64" CACHE STRING "macOS architecture(s) to build for")
    set(CMAKE_OSX_ARCHITECTURES ${MACOS_ARCH})

    # Find required libraries
    find_library(VULKAN_LIBRARY NAMES vulkan PATHS ${CMAKE_CURRENT_SOURCE_DIR}/bin/vulkan/osx)
    find_library(MOLTEN_VK_LIBRARY NAMES MoltenVK PATHS ${CMAKE_CURRENT_SOURCE_DIR}/bin/vulkan/osx)

    if(NOT VULKAN_LIBRARY)
        message(FATAL_ERROR "Vulkan library not found. Please ensure it's installed in the project's bin/vulkan/osx directory.")
    endif()

    if(NOT MOLTEN_VK_LIBRARY)
        message(FATAL_ERROR "MoltenVK library not found. Please ensure it's installed in the project's bin/vulkan/osx directory.")
    endif()

    # Link libraries
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework Foundation"
        "-framework CoreServices"
        "-framework GameController"
        "-framework AppKit"
        ${VULKAN_LIBRARY}
        ${MOLTEN_VK_LIBRARY}
        ${GLFW_LIBRARY}
    )

    add_custom_command(
        TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/vulkan/osx/libvulkan.1.dylib
        ${CMAKE_CURRENT_BINARY_DIR}
    )

    # Copy dylibs to the output directory
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${VULKAN_LIBRARY}
        ${MOLTEN_VK_LIBRARY}
        ${GLFW_LIBRARY}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )

    # Set RPATH for the executable
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "@executable_path"
        BUILD_WITH_INSTALL_RPATH TRUE
    )

elseif(WIN32)
    # Windows specific settings
    # Add Windows specific libraries and include directories
    # You might need to adjust paths based on your Windows setup
    find_library(VULKAN_LIBRARY NAMES vulkan-1 PATHS ${CMAKE_CURRENT_SOURCE_DIR}/bin/vulkan/win)

    if(NOT VULKAN_LIBRARY)
        message(FATAL_ERROR "Vulkan library not found. Please ensure it's installed in the correct directory.")
    endif()

    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${VULKAN_LIBRARY}
        ${GLFW_LIBRARY}
    )
endif()

add_custom_target(
    shaders ALL
    DEPENDS ${SHADER_BINARIES}
    COMMENT "Compiling all shaders"
)

add_dependencies(${PROJECT_NAME} shaders)

# Copy data folder to the output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/data
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/data
)

# Copy compiled shaders to the output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)
