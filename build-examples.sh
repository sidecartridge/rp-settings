#!/bin/bash

# Install SDK needed for building
git submodule init
git submodule update --init --recursive

# Set the environment variables of the SDKs
export PICO_SDK_PATH=$PWD/pico-sdk
# Don't forget to set the path to the picotool
#export PICOTOOL_OVERRIDE_DIR=

VERSION_FILE="version.txt"

# Read the release version from the version.txt file
export RELEASE_VERSION=$(cat "$VERSION_FILE" | tr -d '\r\n ')
echo "Release version: $RELEASE_VERSION"

# Get the release date and time from the current date
export RELEASE_DATE=$(date +"%Y-%m-%d %H:%M:%S")
echo "Release date: $RELEASE_DATE"

# Set the release or debug build type
# If nothing passed as second argument, use release
export BUILD_TYPE=${2:-release}
echo "Build type: $BUILD_TYPE"

# If the build type is release, set DEBUG_MODE environment variable to 0
# Otherwise set it to 1
if [ "$BUILD_TYPE" = "release" ]; then
    export DEBUG_MODE=0
else
    export DEBUG_MODE=1
fi

# Set the build directory. Delete previous contents if any
rm -rf build
mkdir build

# We assume that the last firmware was built for the same board type
# And previously pushed to the repo version

# Build the project
cd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../
make -j4

# Copy the built firmware to the /dist folder
cd ..
mkdir -p dist
if [ "$BUILD_TYPE" = "release" ]; then
    cp build/examples/examples.uf2 dist/settings-examples.uf2
else
    cp build/examples/examples.uf2 dist/settings-examples-$BUILD_TYPE.uf2
fi
