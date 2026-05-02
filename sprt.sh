#!/bin/bash
set -ex

ALPHA=$1
BETA=$2

if [ -z "$ALPHA" || -z "$BETA"  ]; then
  echo "Error: No version argument provided."
  echo "Usage: $0 <alpha> <beta>"
  exit 1
fi

./cmake-build-release/cascade runner "./prog/$ALPHA/cascade" "./prog/$BETA/cascade"