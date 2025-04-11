#!/bin/bash

# Ensure a version is provided
if [ -z "$1" ]; then
  echo "Usage: $0 <version>"
  exit 1
fi

VERSION=$1

# Update the VERSION file
echo "$VERSION" > VERSION
git add VERSION
git commit -m "Release version $VERSION"

# Create a tag and push it
git tag "v$VERSION"
git push origin main
git push origin "v$VERSION"

echo "Release v$VERSION created and pushed."
