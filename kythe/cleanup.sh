#!/bin/sh
set -e

BASE=$(dirname $(dirname $(readlink -f "$0")))

echo "Erasing ${BASE}/kythe_out."
mkdir -p "$BASE/kythe_out";
# We use buster-slim because it's the base image for kythe/Dockerfile
docker run --rm \
       -v "$BASE/kythe_out:/out" \
       debian:buster-slim \
       sh -c "rm -rf /out/*"
