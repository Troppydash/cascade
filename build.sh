#!/bin/bash
set -ex

VER=$1

if [ -z "$VER" ]; then
  echo "Error: No version argument provided."
  echo "Usage: $0 <version_name>"
  exit 1
fi

# build
make clean && make build_release
DEST="./prog/$VER"
if [ -d "$DEST" ]; then
  echo "deleting $DEST"
  rm -rf "$DEST"
fi

echo "copying to $DEST"
mkdir -p "$DEST"
cp ./build/cascade "$DEST/cascade"