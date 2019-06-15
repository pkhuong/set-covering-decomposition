#!/bin/sh

# make sure we own the cache directory before running bazel.
chown $(whoami):$(whoami) -R /cache

# Tail call to the old entrypoint in
# https://github.com/kythe/kythe/blob/master/kythe/extractors/bazel/Dockerfile
exec /kythe/extract.sh "$@"
