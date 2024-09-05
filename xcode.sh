
#!/bin/bash

# Set the build directory name
BUILD_DIR="xcode_build"

# Create and navigate to the xcode build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Generate the Xcode project using CMake
cmake -G Xcode ..