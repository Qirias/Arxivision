#!/bin/bash

# Set the build directory name
BUILD_DIR="general_build"

# Check if the build directory exists and remove it
if [ -d "$BUILD_DIR" ]; then
    echo "Removing existing $BUILD_DIR directory..."
    rm -rf "$BUILD_DIR"
fi

# Create and navigate to the general build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Generate Makefiles or Ninja build system using CMake
cmake ..

# Build the project
cmake --build .
