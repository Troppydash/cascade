#!/bin/bash
set -ex

ALPHA=$1
BETA=$2

./cmake-build-release/cascade runner "./prog/$ALPHA/cascade" "./prog/$BETA/cascade"