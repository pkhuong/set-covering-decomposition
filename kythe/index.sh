#!/bin/sh
set -e

KYTHE=$(dirname $(readlink -f "$0"))
BASE=$(dirname "$KYTHE")

# Build and store extracted commands from bazel.
# https://github.com/kythe/kythe/tree/master/kythe/extractors/bazel
if ! $(docker images | grep -q '^kythe-scan\s')
then
    echo "Building kythe-scan.";
    docker build -t kythe-scan "$KYTHE" -f "$KYTHE/scan.Dockerfile";
fi
SCAN=$(docker build -q -t kythe-scan "$KYTHE" -f "$KYTHE/scan.Dockerfile")

mkdir -p "$BASE/kythe_out/scan"
mkdir -p "$BASE/kythe_out/cache"
mkdir -p "$BASE/kythe_out/index"

echo "Overriding permissions in ${BASE}/kythe_out."
RESTORE=$(docker build -q -t kythe-permissions "$KYTHE" -f "$KYTHE/fix_permissions.Dockerfile")
docker run --rm \
       -v "$BASE/kythe_out:/out" \
       $RESTORE /out

echo "Extracting xref information from the bazel build."
echo "This may fail with network errors. Retry a couple times until the cache is populated."
find "$BASE/kythe_out/scan/" -name "*.kzip" -delete;
docker run --rm \
       -v "$BASE:/workspace/code" \
       -v "$BASE/kythe_out/scan:/workspace/output" \
       -v "$BASE/kythe_out/cache:/cache/" \
       -e KYTHE_OUTPUT_DIRECTORY=/workspace/output \
       -e TERM="$TERM" \
       -w /workspace/code \
       $SCAN build \
       --define kythe_corpus=set-covering-decomposition \
       --define gui=yes \
       //...

# See https://github.com/kythe/kythe/blob/master/kythe/examples/bazel/kythe-index.sh

# The next kythe image has a binary release.
if ! $(docker images | grep -q '^kythe\s')
then
    echo "Building kythe.";
    docker build -t kythe "$KYTHE";
fi
BUILT=$(docker build -q -t kythe "$KYTHE")

# Convert kzip to graphstore.
# TODO: find out if we can split up the kzip and index in
# parallel. Also, why does it sigsegv?
mkdir -p "$BASE/kythe_out/index"
rm -r  "$BASE/kythe_out/index/"* || true;
echo "Indexing extracted kzip from the build scan."
docker run --rm \
       -v "$BASE/kythe_out/scan:/compilations" \
       -v "$BASE/kythe_out/index:/index" \
       $BUILT sh -c \
       "find compilations -name '*\.kzip' -print0 | parallel -0 -L 1 /opt/kythe/indexers/cxx_indexer | /opt/kythe/tools/dedup_stream > index/graphstore"

# Convert graphstore to serving table.
# XXX: disabled experimental pipeline because the experimental stuff is less robust against errors (:
#  --experimental_beam_pipeline --experimental_beam_columnar_data
echo "Assembling serving table."
docker run --rm \
       -v "$BASE/kythe_out/index/:/index" \
       $BUILT /opt/kythe/tools/write_tables \
       --entries "/index/graphstore" \
       --out "/index/serving_table"

echo "Restoring permissions in ${BASE}/kythe_out."
docker run --rm \
       -v "$BASE/kythe_out:/out" \
       $RESTORE /out

echo "Run kythe/serve.sh to serve the current index in ${BASE}/kythe_out/."
