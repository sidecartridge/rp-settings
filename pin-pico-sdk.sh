#!/bin/bash

# Submodule URL and desired tag
SUBMODULE_URL="git@github.com:raspberrypi/pico-sdk.git"
SUBMODULE_PATH="pico-sdk"
SUBMODULE_TAG="2.1.0"

# Check if submodule already exists
if git config --file .gitmodules --get-regexp path | grep -q "$SUBMODULE_PATH"; then
    echo "Submodule already exists. Checking out tag $SUBMODULE_TAG..."
    # Update and checkout the specific tag
    cd "$SUBMODULE_PATH"
    git fetch --tags
    git checkout tags/$SUBMODULE_TAG
    cd ..
else
    echo "Submodule does not exist. Adding..."
    # Add the submodule and checkout the specific tag
    git submodule add --force "$SUBMODULE_URL" "$SUBMODULE_PATH"
    git submodule update --init --recursive 
    cd "$SUBMODULE_PATH"
    git fetch --tags
    git checkout tags/$SUBMODULE_TAG
    cd ..
fi

# Automatically commit the submodule update
git add "$SUBMODULE_PATH"
git commit -m "Pin $SUBMODULE_PATH to tag $SUBMODULE_TAG"

echo "Submodule pinned to tag $SUBMODULE_TAG and committed."
