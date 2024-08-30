#!/bin/bash

# Set the build directory name
BUILD_DIR="general_build"

# Create and navigate to the general build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Generate Makefiles or Ninja build system using CMake
cmake ..

# Build the project
cmake --build .

# Run executable
./Arxivision
