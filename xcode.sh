
#!/bin/bash

# Set the build directory name
BUILD_DIR="xcode_build"

# Check if the build directory exists and remove it
if [ -d "$BUILD_DIR" ]; then
    echo "Removing existing $BUILD_DIR directory..."
    rm -rf "$BUILD_DIR"
fi

# Create and navigate to the xcode build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Generate the Xcode project using CMake
cmake -G Xcode ..

