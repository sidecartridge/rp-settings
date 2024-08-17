#!/bin/bash

# Submodule URL and new desired tag
SUBMODULE_PATH="pico-sdk"
NEW_TAG=$1  # Pass the new tag as an argument, e.g., ./update_submodule.sh 3.0.0

if [ -z "$NEW_TAG" ]; then
    echo "Please provide a tag to update the submodule to."
    exit 1
fi

# Navigate to the submodule directory
cd "$SUBMODULE_PATH"

# Fetch the latest tags and checkout the new tag
git fetch --tags
git checkout tags/$NEW_TAG

# Go back to the main project directory
cd ..

# Stage the submodule update
git add "$SUBMODULE_PATH"

# Commit the submodule update
git commit -m "Update $SUBMODULE_PATH submodule to tag $NEW_TAG"

echo "Submodule updated to tag $NEW_TAG and committed."
