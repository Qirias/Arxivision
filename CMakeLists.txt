cmake_minimum_required(VERSION 3.20)
project(ArXivision)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Suppress warnings for GCC/Clang
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")

# Set CMake to generate Xcode project
set(CMAKE_GENERATOR "Xcode")

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

# Include shader files in the project
file(GLOB_RECURSE SHADER_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${SHADER_SOURCES})

# Set the working directory for the target
set_target_properties(${PROJECT_NAME} PROPERTIES
    XCODE_GENERATE_SCHEME TRUE
    XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Find GLFW
find_library(GLFW_LIBRARY NAMES glfw glfw3 PATHS ${CMAKE_CURRENT_SOURCE_DIR}/bin/glfw/osx)
if(NOT GLFW_LIBRARY)
    message(FATAL_ERROR "GLFW library not found. Please ensure it's installed in the project's bin/glfw/osx directory.")
endif()

# Find GLFW include directory
find_path(GLFW_INCLUDE_DIR NAMES GLFW/glfw3.h PATHS ${CMAKE_CURRENT_SOURCE_DIR}/bin/glfw/osx/include)
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

# Copy libvulkan.1.dylib to the output directory
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/bin/vulkan/osx/libvulkan.1.dylib
    ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>
)

# Copy dylibs to the output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${VULKAN_LIBRARY}
    ${MOLTEN_VK_LIBRARY}
    ${GLFW_LIBRARY}
    ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>
)

# Set RPATH for the executable
set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "@executable_path"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Compile shaders
file(GLOB SHADER_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp)
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
        COMMAND glslc ${SHADER} -o ${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${SHADER_NAME}"
    )
    list(APPEND SHADER_BINARIES ${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv)
endforeach()

add_custom_target(
    shaders ALL
    DEPENDS ${SHADER_BINARIES}
    COMMENT "Compiling all shaders"
)

add_dependencies(${PROJECT_NAME} shaders)

# Copy project structure to the output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/bin
    ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/bin
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/data
    ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/data
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/libs
    ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/libs
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/shaders
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/source
)

# Add project files to source groups to maintain folder structure in Xcode
file(GLOB_RECURSE ALL_SOURCE_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/*.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/*.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.spv
)

foreach(SOURCE_FILE ${ALL_SOURCE_FILES})
    get_filename_component(SOURCE_PATH "${SOURCE_FILE}" PATH)
    file(RELATIVE_PATH SOURCE_PATH_REL "${CMAKE_CURRENT_SOURCE_DIR}" "${SOURCE_PATH}")
    string(REPLACE "/" "\\" SOURCE_PATH_MSVC "${SOURCE_PATH_REL}")
    source_group("${SOURCE_PATH_MSVC}" FILES "${SOURCE_FILE}")
endforeach()

# Set the startup project in Xcode
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
