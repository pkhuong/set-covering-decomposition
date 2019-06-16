#!/bin/sh
set -e;

# make sure we own the cache directory before running bazel.
chown $(whoami):$(whoami) -R /cache;

# Remove old symlinks symlinks
rm bazel-code bazel-bin bazel-genfiles bazel-out bazel-testlogs || true;
# Defer to the areal entrypoint in
# https://github.com/kythe/kythe/blob/master/kythe/extractors/bazel/Dockerfile
/kythe/extract.sh "$@";

# And clean up our new symlinks.
rm bazel-code bazel-bin bazel-genfiles bazel-out bazel-testlogs || true;
